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

template <typename ResData>
class ResManager
{
public:
	using Handle = ResData::Handle;
	inline static constexpr Handle INVALID_HANDLE = ResData::INVALID_HANDLE;
	using Hash = std::size_t;
	inline static constexpr Hash INVALID_HASH = 0;

	class Resource
	{
	public:
		Resource(ResData &&data) : data_(std::move(data)), handle_(INVALID_HANDLE), hash_(INVALID_HASH) {}

		~Resource()
		{
			if (handle_ != INVALID_HANDLE)
				data_.destroyHandle(handle_);
		}

		const ResData &getData() const noexcept
		{
			return data_;
		}

		Handle getHandle() const noexcept
		{
			return handle_ != INVALID_HANDLE ? handle_ : handle_ = data_.createHandle();
		}

		Hash getHash() const noexcept
		{
			return hash_ != INVALID_HASH ? hash_ : hash_ = data_.createHash();
		}

	private:
		ResData data_;
		mutable Handle handle_;
		mutable Hash hash_;
	};

	using RPtr = Resource *;
	using SPtr = std::shared_ptr<Resource>;
	using WPtr = std::weak_ptr<Resource>;

	using Key = RPtr; // Resource *; // ResData*;
	using Type = WPtr;

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
		static ResManager manager_;
		return manager_;
	}

public:
	SPtr assign(ResData &&data)
	{
		std::lock_guard<Mutex> lock(getMutex());
		std::cout << "assign  " << data << " - ";
		auto it = container_.find(&data);
		if (it != container_.end())
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
		container_.insert({res.get(), res});
		return res;
	}

private:
	// ToDo
	Mutex &getMutex()
	{
		return mutex_;
	}

	void release(Resource *resource)
	{
		std::lock_guard<Mutex> lock(getMutex());
		std::cout << "release " << *resource->getData() << " - ";
		auto it = container_.find(resource);
		assert(it != container_.end());
		std::cout << "thread id = " << std::this_thread::get_id() << ", ";
		std::cout << "found, erase it\n";
		container_.erase(it);
		delete resource;
	}

	ResManager() = default;

	~ResManager() = default;

	Mutex mutex_;
	Container container_;
};

#endif // _WIZ_RESMAN_H_