// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::IO;

Async::Async()
{
	_stopThread = false;
	VERUS_ZERO_MEM(_queue);
}

Async::~Async()
{
	Done();
}

void Async::Init()
{
	VERUS_INIT();

	_stopThread = false;
	_thread = std::thread(&Async::ThreadProc, this);
}

void Async::Done()
{
	if (_thread.joinable())
	{
		_stopThread = true;
		_cv.notify_one();
		_thread.join();
	}
	VERUS_DONE(Async);
}

void Async::Load(CSZ url, PAsyncDelegate pDelegate, RcTaskDesc desc)
{
	VERUS_RT_ASSERT(IsInitialized());
	VERUS_RT_ASSERT(!_inUpdate); // Not allowed to call Load() in Update().
	VERUS_RT_ASSERT(url && *url);

	// If the queue is full we must block until something gets processed:
	bool full = false;
	do
	{
		{
			VERUS_LOCK(*this);

			for (auto& kv : _mapTasks)
			{
				if (!strcmp(url, _C(kv.first) + s_orderLength))
				{
					// This resource is already scheduled.
					// Just add a new owner, if it's not already there.
					RTask task = kv.second;
					if (std::find(task._vOwners.begin(), task._vOwners.end(), pDelegate) == task._vOwners.end())
						task._vOwners.push_back(pDelegate);
					return;
				}
			}

			full = VERUS_CIRCULAR_IS_FULL(_cursorRead, _cursorWrite, VERUS_COUNT_OF(_queue));

			if (!full)
			{
				char order[16];
				sprintf_s(order, "%08X", _order); // The order of Load() and delegates is preserved.
				_order++;
				String key = order;
				key += url;

				RTask task = _mapTasks[key];
				task._vOwners.push_back(pDelegate);
				task._desc = desc;
				_queue[_cursorWrite] = _C(_mapTasks.find(key)->first);
				VERUS_CIRCULAR_ADD(_cursorWrite, VERUS_COUNT_OF(_queue));
			}
		}

		if (full)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(290));
			Update();
		}
	} while (full);

	_cv.notify_one();
}

void Async::_Cancel(PAsyncDelegate pDelegate)
{
	VERUS_RT_ASSERT(IsInitialized());
	VERUS_LOCK(*this);
	for (auto& kv : _mapTasks)
	{
		RTask task = kv.second;
		VERUS_WHILE(Vector<PAsyncDelegate>, task._vOwners, it)
		{
			if (*it == pDelegate)
				it = task._vOwners.erase(it);
			else
				++it;
		}
	}
}

void Async::Cancel(PAsyncDelegate pDelegate)
{
	if (IsValidSingleton())
		I()._Cancel(pDelegate);
}

void Async::Update()
{
	VERUS_RT_ASSERT(IsInitialized());
	VERUS_LOCK(*this);

	if (_ex.IsRaised())
		throw _ex;

	_inUpdate = true;
	VERUS_WHILE(TMapTasks, _mapTasks, itTask)
	{
		RTask task = itTask->second;
		if (task._loaded) // Only loaded task can be processed:
		{
#ifdef VERUS_RELEASE_DEBUG
			VERUS_LOG_DEBUG("Update(); task=" << itTask->first);
#endif
			VERUS_RT_ASSERT(task._desc._runOnMainThread);
			if (!task._v.empty())
			{
				for (const auto& pOwner : task._vOwners)
					pOwner->Async_WhenLoaded(_C(itTask->first) + s_orderLength, Blob(task._v.data(), task._v.size()));
			}
			itTask = _mapTasks.erase(itTask);
			// Task complete.
			if (_onePerUpdateMode)
				break;
		}
		else
			++itTask;
	}
	if (_mapTasks.empty())
		_order = 0;
	_inUpdate = false;

	if (_flush) // Reset timer after flushing (update can take a long time):
	{
		VERUS_QREF_TIMER;
		timer.Update();
		timer.Update();
		_flush = false;
	}
}

void Async::Flush()
{
	while (true)
	{
		{
			VERUS_LOCK(*this);
			if (_ex.IsRaised())
				throw _ex;
			if (_cursorRead == _cursorWrite)
				return;
			_flush = true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

void Async::ThreadProc()
{
	VERUS_RT_ASSERT(IsInitialized());
	try
	{
		SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
		while (true)
		{
			CSZ key = nullptr;
			PTask pTask = nullptr; // Assume that this address will not change in the map.
			TMapTasks::iterator itTask;

			{
				// Get the next task:
				VERUS_LOCK(*this);

				// If there are no tasks, unlock and wait:
				_cv.wait(lock, [=]() {return (_cursorRead != _cursorWrite) || _stopThread; });

				if (_stopThread)
					break; // Quit.

				// Locked and should have a task:
				itTask = _mapTasks.end();
				key = _queue[_cursorRead];
				if (key)
				{
					itTask = _mapTasks.find(key);
					if (itTask != _mapTasks.end())
						pTask = &_mapTasks[key];
				}
			}

			if (key && (!pTask->_desc._checkExist || FileSystem::FileExist(key + s_orderLength)))
			{
				try
				{
					FileSystem::LoadResource(key + s_orderLength, pTask->_v,
						FileSystem::LoadDesc(pTask->_desc._nullTerm, pTask->_desc._texturePart));
				}
				catch (D::RcRuntimeError)
				{
					if (!pTask->_desc._checkExist)
						throw;
				}
			}

#ifdef VERUS_RELEASE_DEBUG
			VERUS_LOG_DEBUG("ThreadProc(); key=" << key);
#endif

			{
				// Finalize this task:
				VERUS_LOCK(*this);
				pTask->_loaded = true;
				// Is it safe to call a delegate on this loader thread?
				if (!pTask->_desc._runOnMainThread)
				{
#ifdef VERUS_RELEASE_DEBUG
					VERUS_LOG_DEBUG("ThreadProc(); runOnMainThread key=" << key);
#endif
					if (!pTask->_v.empty())
					{
						for (const auto& pOwner : pTask->_vOwners)
							pOwner->Async_WhenLoaded(key + s_orderLength, Blob(pTask->_v.data(), pTask->_v.size()));
					}
					_mapTasks.erase(itTask);
					// Task complete.
					if (_mapTasks.empty())
						_order = 0;
				}
				VERUS_CIRCULAR_ADD(_cursorRead, VERUS_COUNT_OF(_queue));
			}
		}
	}
	catch (D::RcRuntimeError e)
	{
		VERUS_LOCK(*this);
		_ex = e;
	}
	catch (const std::exception& e)
	{
		VERUS_LOCK(*this);
		_ex = VERUS_RUNTIME_ERROR << e.what();
	}
}
