#pragma once

#include <Core/Core.h>

#include <dxcapi.h>

// for use in RHIObject inheritance chains
#define GENERATE_RHI_OBJECT_BODY_NO_RHI() \
private: \
	std::atomic<int32_t> m_ref_counter = 0; \
public: \
	virtual void AddRef() override; \
	virtual void Release() override; \
private:


#define IMPLEMENT_RHI_OBJECT(ClassName) \
	void ClassName::AddRef() { m_ref_counter++; } \
	void ClassName::Release() { if (--m_ref_counter <= 0) m_rhi->DeferredDestroyRHIObject(this); }

template<typename T>
struct RHIImplClass
{
	using Type = void;
};

template<typename T>
RHIImplClass<T>::Type& RHIImpl(T& obj) { return static_cast<RHIImplClass<T>::Type&>(obj); }

template<typename T>
const typename RHIImplClass<T>::Type& RHIImpl(const T& obj) { return static_cast<const RHIImplClass<T>::Type&>(obj); }

template<typename T>
RHIImplClass<T>::Type* RHIImpl(T* obj) { return static_cast<RHIImplClass<T>::Type*>(obj); }

template<typename T>
const typename RHIImplClass<T>::Type* RHIImpl( const T* obj ) { return static_cast<const RHIImplClass<T>::Type*>( obj ); }

#define IMPLEMENT_RHI_INTERFACE(InterfaceType, ImplementationType) \
template<> struct RHIImplClass<InterfaceType> \
{ \
	using Type = ImplementationType; \
};