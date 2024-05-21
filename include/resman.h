#pragma once
#ifndef _WIZ_RESMAN_H_
#define _WIZ_RESMAN_H_

#include <assert.h>
#include <iostream>
#include <sstream>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>

template <typename ResData, typename Handle = unsigned int, Handle INVALID_HANDLE = 0>
class ResManager
{
public:
	using Hash = std::size_t;
	inline static constexpr Hash INVALID_HASH = 0;

	class Resource
	{
	public:
		Resource(ResData &&data) : _data(std::move(data)), _handle(INVALID_HANDLE), _hash(INVALID_HASH) {}
		~Resource()
		{
			if (_handle != INVALID_HANDLE)
				_data.destroyHandle(_handle);
		}
		ResData *getData() { return &_data; }
		Handle getHandle()
		{
			return _handle != INVALID_HANDLE ? _handle : _handle = _data.createHandle();
		}
		Hash getHash()
		{
			return _hash != INVALID_HASH ? _hash : _hash = _data.createHash();
		}

	private:
		ResData _data;
		Handle _handle;
		Hash _hash;
	};

	using Str = std::shared_ptr<Resource>;
	using Wtr = std::weak_ptr<Resource>;

	using Key = Resource *; // ResData*;
	using Type = Wtr;

	struct Hasher
	{
		using is_transparent = void;
		auto operator()(Resource *resource) const noexcept
		{
			return resource->getHash();
		}
		auto operator()(ResData *data) const noexcept
		{
			return data->createHash();
		}
	};

	struct KeyEqual
	{
		using is_transparent = void;
		bool operator()(Resource *left, Resource *right) const noexcept
		{
			return left->getData()->operator==(*right->getData());
		}
		// bool operator()(Resource* resource, ResData* data) const noexcept {
		//	return data->operator==(*resource->getData());
		// }
		bool operator()(ResData *data, Resource *resource) const noexcept
		{
			return data->operator==(*resource->getData());
		}
	};

	using Container = std::unordered_map<Key, Type, Hasher, KeyEqual>;
	using Mutex = std::mutex;

	static ResManager &get()
	{
		static ResManager manager;
		return manager;
	}

	Str assign(ResData &&data)
	{
		std::lock_guard<Mutex> lock(getMutex());
		std::cout << "assign  " << data << " - ";
		auto it = _container.find(&data);
		if (it != _container.end())
		{
			std::cout << "found, return it\n";
			return it->second.lock();
		}
		std::cout << "not found, insert it\n";
		// auto res = std::make_shared<T>(data);
		auto res = Str(
			new Resource(std::move(data)),
			[](Resource *resource)
			{
				ResManager::get().release(resource);
			});
		_container.insert({res.get(), res});
		return res;
	}

private:
	static Mutex &getMutex()
	{
		static Mutex mutex;
		return mutex;
	}

	void release(Resource *resource)
	{
		std::lock_guard<Mutex> lock(getMutex());
		std::cout << "release " << *resource->getData() << " - ";
		auto it = _container.find(resource);
		assert(it != _container.end());
		std::cout << "thread id = " << std::this_thread::get_id() << ", ";
		std::cout << "found, erase it\n";
		_container.erase(it);
		delete resource;
	}

	ResManager() = default;
	~ResManager() = default;
	Container _container;
};

#endif // _WIZ_RESMAN_H_