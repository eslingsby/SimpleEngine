#pragma once

#include "ObjectPool.hpp"
#include "TypeMask.hpp"
#include "Utility.hpp"

#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <tuple>

template <uint32_t typeWidth>
class EntityManager {
	using TypeMask = TypeMask<typeWidth>;

	struct Identity {
		enum Flags : uint8_t {
			Empty = 0x00,
			Active = 0x01,
			Enabled = 0x02,
			Erased = 0x04,
			Buffered = 0x08,
		};

		uint32_t index;
		uint32_t version;
		TypeMask mask;

		uint8_t flags = 0;
		uint32_t references = 0;
	};

	const size_t _chunkSize;

	BasePool* _pools[typeWidth] = { nullptr };

	std::vector<Identity> _identities;
	std::vector<uint32_t> _freeIndexes;

	std::vector<uint32_t> _buffered;

	void* _enginePtr = nullptr;

	bool _iterating = false;

	bool _validId(uint32_t index, uint32_t version) const;

	void _erase(uint32_t index);

	template <typename ...Ts, typename T>
	void _iterate(const Identity& identity, const T& lambda);

	template <uint32_t i, typename ...Ts>
	inline typename std::enable_if<i == sizeof...(Ts)>::type _getI(uint32_t index, std::tuple<Ts*...>& tuple);

	template <uint32_t i, typename ...Ts>
	inline typename std::enable_if<i < sizeof...(Ts)>::type _getI(uint32_t index, std::tuple<Ts*...>& tuple);

	template <uint32_t i, typename ...Ts>
	inline typename std::enable_if<i == sizeof...(Ts)>::type _reserve(uint32_t index);

	template <uint32_t i, typename ...Ts>
	inline typename std::enable_if<i < sizeof...(Ts)>::type _reserve(uint32_t index);

	template <typename T, typename T1, typename ...Ts>
	inline typename std::enable_if<std::is_pointer<T>::value == false && std::is_constructible<T, EntityManager<typeWidth>&, uint64_t, Ts...>::value>::type _insert(uint32_t index, Ts&&... args);

	template <typename T, typename T1, typename ...Ts>
	inline typename std::enable_if<std::is_pointer<T>::value == false && std::is_constructible<T, Ts...>::value>::type _insert(uint32_t index, Ts&&... args);

	template <typename T, typename T1, typename ...Ts>
	inline typename std::enable_if<std::is_pointer<T>::value == true && std::is_constructible<T1, EntityManager<typeWidth>&, uint64_t, Ts...>::value>::type _insert(uint32_t index, Ts&&... args);

	template <typename T, typename T1, typename ...Ts>
	inline typename std::enable_if<std::is_pointer<T>::value == true && std::is_constructible<T1, Ts...>::value>::type _insert(uint32_t index, Ts&&... args);

	template <typename T>
	inline bool _get(uint64_t id);

	template <typename T, typename T1, typename ...Ts>
	inline bool _add(uint64_t id, Ts&&... args);

public:
	inline EntityManager(size_t chunkSize);

	inline ~EntityManager();

	inline bool valid(uint64_t id) const;

	inline uint64_t create();

	inline void erase(uint64_t id);

	template <typename T>
	inline typename std::enable_if<std::is_pointer<T>::value == false, T*>::type get(uint64_t id);

	template <typename T>
	inline typename std::enable_if<std::is_pointer<T>::value == true, T>::type get(uint64_t id);

	template <typename T, typename ...Ts>
	inline typename std::enable_if<std::is_pointer<T>::value == false, T*>::type add(uint64_t id, Ts&&... args);

	template <typename T, typename ...Ts>
	inline typename std::enable_if<std::is_pointer<T>::value == true, T>::type add(uint64_t id, Ts&&... args);

	template <typename T, typename T1, typename ...Ts>
	inline typename std::enable_if<std::is_pointer<T>::value == true, T1*>::type add(uint64_t id, Ts&&... args);

	template <typename T>
	inline void remove(uint64_t id);

	template <typename ...Ts>
	inline bool has(uint64_t id) const;

	template <typename ...Ts>
	inline void reserve(uint32_t count);

	inline void clear();

	inline uint32_t count() const;

	template <typename ...Ts, typename T>
	inline void iterate(const T& lambda);

	inline void reference(uint64_t id);

	inline void dereference(uint64_t id);

	inline void setEnabled(uint64_t id, bool enabled);

	inline void enginePtr(void* engine);

	inline void* enginePtr();
};

template<uint32_t typeWidth>
bool EntityManager<typeWidth>::_validId(uint32_t index, uint32_t version) const{
	if (index >= _identities.size())
		return false;

	return version == _identities[index].version;
}

template <uint32_t typeWidth>
void EntityManager<typeWidth>::_erase(uint32_t index) {
	assert(hasFlags(_identities[index].flags, Identity::Active)); // sanity
	
	// if references still exist, mark as erased for later, and return
	if (_identities[index].references) {
		_identities[index].flags |= Identity::Erased;
		return;
	}

	// remove components from each pool
	for (uint32_t i = 0; i < typeWidth; i++) {
		if (_identities[index].mask.has(i)) {
			assert(_pools[i]); // sanity
			_pools[i]->erase(index);
			_identities[index].mask.sub(i);
		}
	}

	// clear up identity, increment version
	_identities[index].version++;
	_identities[index].flags = Identity::Empty;

	_freeIndexes.push_back(index);
}

template<uint32_t typeWidth>
template<typename ...Ts, typename T>
inline void EntityManager<typeWidth>::_iterate(const Identity& identity, const T& lambda){
	// skip if not active and enabled
	if (!hasFlags(identity.flags, Identity::Active | Identity::Enabled))
		return;

	// skip if erased or buffered
	if (hasFlags(identity.flags, Identity::Erased) || hasFlags(identity.flags, Identity::Buffered))
		return;

	// skip if entity doesn't have components
	if (!identity.mask.has<Ts...>())
		return;

	// get components and call lambda
	std::tuple<Ts*...> components;
	_getI<0>(identity.index, components);

	lambda(combine32(identity.index, identity.version), *std::get<Ts*>(components)...);
}

template<uint32_t typeWidth>
template <uint32_t i, typename ...Ts>
typename std::enable_if<i == sizeof...(Ts)>::type EntityManager<typeWidth>::_getI(uint32_t index, std::tuple<Ts*...>& tuple) { }

template<uint32_t typeWidth>
template <uint32_t i, typename ...Ts>
typename std::enable_if<i < sizeof...(Ts)>::type EntityManager<typeWidth>::_getI(uint32_t index, std::tuple<Ts*...>& tuple) {
	using T = typename std::tuple_element<i, std::tuple<Ts...>>::type;

	std::get<i>(tuple) = _pools[TypeMask::index<T>()]->get<T>(index);

	_getI<i + 1>(index, tuple);
}

template<uint32_t typeWidth>
template <uint32_t i, typename ...Ts>
typename std::enable_if<i == sizeof...(Ts)>::type EntityManager<typeWidth>::_reserve(uint32_t index) { }

template<uint32_t typeWidth>
template <uint32_t i, typename ...Ts>
typename std::enable_if<i < sizeof...(Ts)>::type EntityManager<typeWidth>::_reserve(uint32_t index) {
	using T = std::tuple_element<i, std::tuple<Ts...>>::type;

	if (_pools[TypeMask::index<T>()] == nullptr)
		_pools[TypeMask::index<T>()] = new ObjectPool<T>(_chunkSize);

	_pools[TypeMask::index<T>()]->reserve(index);

	_reserve<i + 1, T>(index);
}

template<uint32_t typeWidth>
template <typename T, typename T1, typename ...Ts>
typename std::enable_if<std::is_pointer<T>::value == false && std::is_constructible<T, EntityManager<typeWidth>&, uint64_t, Ts...>::value>::type EntityManager<typeWidth>::_insert(uint32_t index, Ts&&... args) {
	_pools[TypeMask::index<T>()]->insert<T>(index, *this, combine32(index, _identities[index].version), std::forward<Ts>(args)...);
}

template<uint32_t typeWidth>
template <typename T, typename T1, typename ...Ts>
typename std::enable_if<std::is_pointer<T>::value == false && std::is_constructible<T, Ts...>::value>::type EntityManager<typeWidth>::_insert(uint32_t index, Ts&&... args) {
	_pools[TypeMask::index<T>()]->insert<T>(index, std::forward<Ts>(args)...);
}

template<uint32_t typeWidth>
template <typename T, typename T1, typename ...Ts>
typename std::enable_if<std::is_pointer<T>::value == true && std::is_constructible<T1, EntityManager<typeWidth>&, uint64_t, Ts...>::value>::type EntityManager<typeWidth>::_insert(uint32_t index, Ts&&... args) {
	_pools[TypeMask::index<T>()]->insert<T, T1>(index, *this, combine32(index, _identities[index].version), std::forward<Ts>(args)...);
}

template<uint32_t typeWidth>
template <typename T, typename T1, typename ...Ts>
typename std::enable_if<std::is_pointer<T>::value == true && std::is_constructible<T1, Ts...>::value>::type EntityManager<typeWidth>::_insert(uint32_t index, Ts&&... args) {
	_pools[TypeMask::index<T>()]->insert<T, T1>(index, std::forward<Ts>(args)...);
}

template<uint32_t typeWidth>
template <typename T>
bool EntityManager<typeWidth>::_get(uint64_t id) {
	uint32_t index = front64(id);
	uint32_t version = back64(id);

	if (!_validId(index, version))
		return false;

	uint32_t type = TypeMask::index<T>();

	if (_pools[type] == nullptr)
		return false;

	assert(hasFlags(_identities[index].flags, Identity::Active)); // sanity

	if (!_identities[index].mask.has<T>())
		return false;

	return true;
}

template<uint32_t typeWidth>
template <typename T, typename T1, typename ...Ts>
bool EntityManager<typeWidth>::_add(uint64_t id, Ts&&... args) {
	uint32_t index = front64(id);
	uint32_t version = back64(id);

	assert(_validId(index, version) && "calling add with invalid id");

	if (!_validId(index, version))
		return false;

	assert(hasFlags(_identities[index].flags, Identity::Active)); // sanity

	uint32_t type = TypeMask::index<T>();

	if (_identities[index].mask.has<T>())
		return true;

	// update identity
	_identities[index].mask.add<T>();

	// create pool if it doesn't exist
	if (_pools[type] == nullptr)
		_pools[type] = new ObjectPool<T>(_chunkSize);

	_insert<T, T1, Ts...>(index, std::forward<Ts>(args)...);
	return true;
}

template<uint32_t typeWidth>
EntityManager<typeWidth>::EntityManager(size_t chunkSize) : _chunkSize(chunkSize) { }

template <uint32_t typeWidth>
EntityManager<typeWidth>::~EntityManager() {
	clear();

	for (uint32_t i = 0; i < typeWidth; i++) {
		if (_pools[i])
			delete _pools[i];
	}
}

template<uint32_t typeWidth>
inline bool EntityManager<typeWidth>::valid(uint64_t id) const {
	if (id == 0)
		return false;

	uint32_t index = front64(id);
	uint32_t version = back64(id);

	return _validId(index, version);
}

template <uint32_t typeWidth>
uint64_t EntityManager<typeWidth>::create() {
	// find free index
	uint32_t index;

	if (_freeIndexes.empty()) {
		index = static_cast<uint32_t>(_identities.size());
		_identities.push_back({ index, 1 });
	}
	else {
		index = *_freeIndexes.rbegin();
		_freeIndexes.pop_back();
	}

	// set identity flags to active, and enabled by default
	assert(!_identities[index].references); // sanity
	assert(!hasFlags(_identities[index].flags, Identity::Active)); // sanity
	_identities[index].flags = Identity::Active | Identity::Enabled;

	// if made during iteration, buffer for later
	if (_iterating) {
		_identities[index].flags |= Identity::Buffered;
		_buffered.push_back(index);
	}

	// return index and version combined
	return combine32(index, _identities[index].version);
}

template <uint32_t typeWidth>
void EntityManager<typeWidth>::erase(uint64_t id) {
	uint32_t index = front64(id);
	uint32_t version = back64(id);

	assert(_validId(index, version) && "calling erase with invalid id");

	if (!_validId(index, version)) 
		return;

	_erase(index);
}

template <uint32_t typeWidth>
template <typename T>
typename std::enable_if<std::is_pointer<T>::value == false, T*>::type EntityManager<typeWidth>::get(uint64_t id) {
	if (!_get<T>(id))
		return nullptr;

	return _pools[TypeMask::index<T>()]->get<T>(front64(id));
}

template <uint32_t typeWidth>
template <typename T>
typename std::enable_if<std::is_pointer<T>::value == true, T>::type EntityManager<typeWidth>::get(uint64_t id) {
	if (!_get<T>(id))
		return nullptr;

	return *_pools[TypeMask::index<T>()]->get<T>(front64(id));
}

template <uint32_t typeWidth>
template <typename T, typename ...Ts>
typename std::enable_if<std::is_pointer<T>::value == false, T*>::type EntityManager<typeWidth>::add(uint64_t id, Ts&&... args) {
	if (!_add<T, typename std::remove_pointer<T>::type>(id, std::forward<Ts>(args)...))
		return nullptr;

	return _pools[TypeMask::index<T>()]->get<T>(front64(id));
}

template <uint32_t typeWidth>
template <typename T, typename ...Ts>
typename std::enable_if<std::is_pointer<T>::value == true, T>::type EntityManager<typeWidth>::add(uint64_t id, Ts&&... args) {
	if (!_add<T, typename std::remove_pointer<T>::type>(id, std::forward<Ts>(args)...))
		return nullptr;

	return _pools[TypeMask::index<T>()]->get<T>(front64(id));
}

template <uint32_t typeWidth>
template <typename T, typename T1, typename ...Ts>
typename std::enable_if<std::is_pointer<T>::value == true, T1*>::type EntityManager<typeWidth>::add(uint64_t id, Ts&&... args) {
	if (!_add<T, T1>(id, std::forward<Ts>(args)...))
		return nullptr;

	return reinterpret_cast<T1*>(_pools[TypeMask::index<T>()]->get<T>(front64(id)));
}

template <uint32_t typeWidth>
template <typename T>
void EntityManager<typeWidth>::remove(uint64_t id) {
	uint32_t index = front64(id);
	uint32_t version = back64(id);

	if (!_validId(index, version))
		return;

	uint32_t type = TypeMask::index<T>();

	assert(_pools[type] != nullptr); // sanity
	assert(hasFlags(_identities[index].flags, Identity::Active)); // sanity

	if (!_identities[index].mask.has<T>())
		return;

	// remove from pool
	_pools[type]->erase(index);

	// update identity
	_identities[index].mask.sub<T>();
}

template <uint32_t typeWidth>
template <typename ...Ts>
bool EntityManager<typeWidth>::has(uint64_t id) const {
	uint32_t index = front64(id);
	uint32_t version = back64(id);

	assert(_validId(index, version) && "calling has with invalid id");

	if (!_validId(index, version))
		return false;

	if (!hasFlags(_identities[index].flags, Identity::Active))
		return false;

	return _identities[index].mask.has<Ts...>();
}

template<uint32_t typeWidth>
template <typename ...Ts>
void EntityManager<typeWidth>::reserve(uint32_t count) {
	_reserve<0, Ts...>(count - 1);
}

template<uint32_t typeWidth>
void EntityManager<typeWidth>::clear() {
	for (uint32_t i = 0; i < _identities.size(); i++) {
		if (_identities[i].mask.empty() || !hasFlags(_identities[i].flags, Identity::Active))
			continue;

		_erase(i);
	}
}

template <uint32_t typeWidth>
uint32_t EntityManager<typeWidth>::count() const {
	return (_identities.size() + _buffered.size()) - _freeIndexes.size();
}

template <uint32_t typeWidth>
template <typename ...Ts, typename T>
void EntityManager<typeWidth>::iterate(const T& lambda) {
	std::tuple<Ts*...> components;

	// if already iterating (in case of nested iterate)
	bool iterating = _iterating;

	if (!iterating)
		_iterating = true;

	// main iteration
	for (const Identity& i : _identities)
		_iterate<Ts...>(i, lambda);

	// iterate over buffered
	while (_buffered.size()) {
		Identity& identity = _identities[_buffered.front()];		

		_iterate<Ts...>(identity, lambda);

		identity.flags &= ~Identity::Buffered;
		_buffered.erase(_buffered.begin());
	}

	if (!iterating)
		_iterating = false;
}

template <uint32_t typeWidth>
void EntityManager<typeWidth>::reference(uint64_t id) {
	uint32_t index = front64(id);
	uint32_t version = back64(id);

	assert(_validId(index, version) && "calling reference with invalid id");

	if (!_validId(index, version)) 
		return;

	assert(hasFlags(_identities[index].flags, Identity::Active)); // sanity
	
	_identities[index].references++;
}

template <uint32_t typeWidth>
void EntityManager<typeWidth>::dereference(uint64_t id) {
	uint32_t index = front64(id);
	uint32_t version = back64(id);

	assert(_validId(index, version) && "calling dereference with invalid id");

	if (!_validId(index, version))
		return;

	assert(hasFlags(_identities[index].flags, Identity::Active)); // sanity;

	assert(_identities[index].references && "calling dereference with no more references");

	if (!_identities[index].references) 
		return;

	_identities[index].references--;

	if (_identities[index].references == 0 && hasFlags(_identities[index].flags, Identity::Erased))
		_erase(index);
}

template <uint32_t typeWidth>
void EntityManager<typeWidth>::setEnabled(uint64_t id, bool enabled) {
	uint32_t index = front64(id);
	uint32_t version = back64(id);

	assert(_validId(index, version) && "calling setEnabled with invalid id");

	if (!_validId(index, version))
		return;

	assert(hasFlags(_identities[index].flags, Identity::Active)); // sanity;

	if (!enabled)
		_identities[index].flags &= ~Identity::Enabled;
	else
		_identities[index].flags |= Identity::Enabled;
}

template<uint32_t typeWidth>
inline void EntityManager<typeWidth>::enginePtr(void * engine) {
	assert(!_enginePtr); // sanity;

	if (!_enginePtr)
		_enginePtr = engine;
}

template<uint32_t typeWidth>
inline void * EntityManager<typeWidth>::enginePtr() {
	assert(_enginePtr); // sanity;

	return _enginePtr;
}