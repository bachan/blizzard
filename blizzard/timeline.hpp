#ifndef __BLIZZARD_TIMELINE_HPP__
#define __BLIZZARD_TIMELINE_HPP__

#include <stdint.h>
#include <pthread.h>
#include <Judy.h>

class timeline
{
	// mutable pthread_mutex_t mutex;

	Pvoid_t timeline_data;
	Pvoid_t object2time;
	int	time_granularity;

	Word_t get_slot(Word_t object_time)const
	{
		return object_time - object_time % time_granularity;
	}

public:
	struct iterator
	{
		Word_t off1;
		Word_t off2;

		iterator() : off1(0), off2(0){}
		void reset(){off1 = off2 = 0;}
	};

	explicit timeline(int tg) : timeline_data(0), object2time(0), time_granularity(tg)
	{
		// pthread_mutex_init(&mutex, 0);
	}

	~timeline()
	{
		// pthread_mutex_destroy(&mutex);
		clear();
	}

	void clear()
	{
		// pthread_mutex_lock(&mutex);

		if (object2time)
		{
			JudyLFreeArray(&object2time, 0);

			object2time = 0;
		}

		if (timeline_data)
		{
			Word_t key = 0;
			PPvoid_t slot = JudyLFirst(timeline_data, &key, 0);

			while (slot)
			{
				Judy1FreeArray(slot, 0);

				slot = JudyLNext(timeline_data, &key, 0);
			}

			JudyLFreeArray(&timeline_data, 0);

			timeline_data = 0;
		}

		// pthread_mutex_unlock(&mutex);
	}

	void reg(Word_t object, Word_t object_time)
	{
		// pthread_mutex_lock(&mutex);

		PWord_t h = (PWord_t)JudyLIns(&object2time, object, 0);
		if (h)
		{
			if (*h)
			{
				Word_t old_time = *h;

				//delete old entry
				Word_t old_key = get_slot(old_time);

				PPvoid_t old_slot = JudyLGet(timeline_data, old_key, 0);
				if (old_slot)
				{
					Judy1Unset(old_slot, object, 0);

					if (Judy1Count(old_slot, 0, -1, 0) == 0)
					{
						JudyLDel(&timeline_data, old_key, 0);
					}
				}
			}

			*h = object_time;

			Word_t new_key = get_slot(object_time);

			PPvoid_t slot = JudyLIns(&timeline_data, new_key, 0);
			if (slot)
			{
				Judy1Set(slot, (Word_t)object, 0);
			}
		}

		// pthread_mutex_unlock(&mutex);
	}

	void del(Word_t object)
	{
		// pthread_mutex_lock(&mutex);

		PPvoid_t h = JudyLGet(object2time, object, 0);
		if (h)
		{
			if (*h)
			{
				Word_t time = (Word_t)*h;

				Word_t key = get_slot(time);

				PPvoid_t slot = JudyLGet(timeline_data, key, 0);
				if (slot)
				{
					Judy1Unset(slot, object, 0);

					if (Judy1Count(slot, 0, -1, 0) == 0)
					{
						JudyLDel(&timeline_data, key, 0);
					}
				}
			}

			JudyLDel(&object2time, object, 0);
		}

		// pthread_mutex_unlock(&mutex);
	}

	void erase_oldest(Word_t term_time)
	{
		// pthread_mutex_lock(&mutex);

		Word_t key = 0;
		PPvoid_t slot = JudyLFirst(timeline_data, &key, 0);

		while (slot && key < get_slot(term_time))
		{
			Word_t obj = 0;
			int rc = Judy1First(*slot, &obj, 0);
			while (rc)
			{
				JudyLDel(&object2time, obj, 0);

				rc = Judy1Next(*slot, &obj, 0);
			}

			Judy1FreeArray(slot, 0);

			JudyLDel(&timeline_data, key, 0);

			slot = JudyLNext(timeline_data, &key, 0);
		}

		// pthread_mutex_unlock(&mutex);
	}

	bool enumerate(iterator& it, Word_t& d, Word_t term_time)const
	{
		// pthread_mutex_lock(&mutex);

		PPvoid_t slot = it.off1 ? JudyLGet(timeline_data, it.off1, 0) : JudyLFirst(timeline_data, &it.off1, 0);

		while (slot && it.off1 < get_slot(term_time))
		{
			bool res = it.off2 ? Judy1Next(*slot, &it.off2, 0) != 0 : Judy1First(*slot, &it.off2, 0) != 0;

			if (res)
			{
				d = it.off2;

				//pthread_mutex_unlock(&mutex);

				return true;
			}
			else
			{
				slot = JudyLNext(timeline_data, &it.off1, 0);

				it.off2 = 0;
			}
		}

		// pthread_mutex_unlock(&mutex);

		return false;
	}
};

#endif /* __BLIZZARD_TIMELINE_HPP__ */
