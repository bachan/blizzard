# Blizzard

Blizzard is a multi-thread HTTP-server with two pools of threads (one for easy to handle requests, and one for hard to handle), forked from Begun's lizard

Blizzard provides an implementation of HTTP-server for developer, who need in
HTTP interface for their applications.

## Usage of blizzard:

```
  -c, --config-file=FILE   - config in FILE
  -p, --pid-file=FILE      - use pid-file FILE
  -d, --daemon             - run as daemon
  -D, --no-daemon          - run as user application
```

## Interface
Blizzard requires a plugin as a shared library with implemented virtual class
`blzmod_sync`. There have to be implemented pure virtual functions:

* `load`: loader of module

* `easy`: handler for easy queue

* `hard`: handler for hard queue

Other functions such as "`idle`" could be implemented optionally.

See a header `blizzard/plugin.hpp` for detailed information about interface `blzmod_sync`.

## Workflow
General workflow of blizzard server:

The Server starts, parses his config, create objects of plugin, call load(),
and inits system objects:

* socket. There listen to the socket; for case of new connections socket will be
  added to list of ready sockets; all network events will be processed; HTTP-requests will be parsed
  and put to easy-queue; all responses from "done" queue will be sent.
  If easy/hard queue is limited, than server will response 503 error to all
  requests over limit.

* easy-threads (number is specified by config). This threads sleep while easy
  queue is empty. If new requests come the easy threads awake with a concurrency
  call of `easy` handler for each request. There is 3 possible results
  for easy handler:

    1. handler processed request: blizzard puts the request to done-queue
    2. handler did not processed: blizzard puts the request to hard-queue
    3. handler found a failure:   blizzard answers HTTP 503 to done-queue

* hard-threads (number is specified by config). This threads sleep while
  hard queue is empty. If new requests come, hard-threads awake with
  concurrency calls of `hard` handler for each request. There is 2 possible
  result for hard handler:

    1. handler processed request: blizzard puts the request to done-queue
    2. handler found a failure:   blizzard answers HTTP 503 to done-queue

* idle-thread. This thread calls idle-function with specified interval if a param `<idle_timeout>` is specified in config.
  The method `idle` will be called once if the time-out is not specified or negative.


## Config

Params:

```
  <pid_file_name>        - name of pid file
  <log_file_name>        - name of log file
  <log_level>            - log level. Possible choice (from critical to less important):
                                alert, crit, error, warn, notice, info, debug

  <stats>
    <uri>                - URI to get stats
  </stats>

  <plugin>
    <ip>                 - IP of listen socket
    <port>               - port to listen
    <connection_timeout> - time-out for connection
    <idle_timeout>       - time interval for idle() in ms
    <library>            - path to .so-plugin
    <params>             - string, specified in set_param for init pluging
    <easy_threads>       - number of easy threads
    <hard_threads>       - number of hard threads
    <easy_queue_limit>   - limit of number request in easy-queue if specified
    <hard_queue_limit>   - limit of number request in hard-queue if specified
  </plugin>
```

## Stats

Stats is provided by URI in the section "stats".
Stats represents a xml:

```
  <blizzard_stats>
      <blizzard_version>0.3.2</blizzard_version>
      <uptime>287</uptime>                     # uptime in seconds
      <rps>1105.250</rps>                      # request per seconds
      <queues>                                 # queue size
          <easy>2</easy>                       # instantaneous size
          <max_easy>1</max_easy>               # maximal size in last 4 seconds
          <hard>0</hard>
          <max_hard>0</max_hard>
          <done>0</done>
          <max_done>1</max_done>
      </queues>
      <response_time>                          # time of processing request in seconds
          <min>0.127500</min>                  # minimal in last 4 seconds
          <avg>1.342132</avg>                  # average in last 4 seconds
          <max>8.674405</max>                  # maximal in last 4 seconds
      </response_time>
      <mem_allocator>                          # information about mem allocation
          <pages>1</pages>                     # allocated pages in HTTP pool
          <objects>8</objects>                 # allocated objects in HTTP pool
      </mem_allocator>
      <rusage>                                 # rusage of blizzard-а
          <utime>2</utime>                     # userspace time
          <stime>4</stime>                     # system time
      </rusage>
  </blizzard_stats>
```

## Bugs

* In case of limit for a number of open file descriptor is too low (such as 1024) and
  amount of requests is high (comparable with limit) there is possible disconnects.

  In case of time-out happens there is need to increase `ulimit -n` to higher value: 16384 or more

* blizzard aggressively responses to incorrect requests: closes connection in case of
  HTTP-request is incorrect; HTTP headers is longer than 8kb; time-out happens; etc
