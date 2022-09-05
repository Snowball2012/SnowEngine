#pragma once

#define NOMINMAX

#include <array>
#include <algorithm>
#include <iostream>
#include <vector>
#include <optional>
#include <span>
#include <set>
#include <queue>

#include <boost/container/small_vector.hpp>

#include <windows.h>

#include <utils/packed_freelist.h>

// double extension trick to stringify __LINE__ and other things
#define S(x) #x
#define S_(x) S(x)

#ifdef NDEBUG
#define INSERT_DBGBREAK
#else
#define INSERT_DBGBREAK DebugBreak();
#endif

#define VERIFY_EQUALS(x, ExpectedValue) \
if ((x) != (ExpectedValue)) \
{ \
	INSERT_DBGBREAK \
	throw std::runtime_error("Failed call to " #x " at " __FILE__ ":" S_(__LINE__) ", expected " #ExpectedValue); \
}

#define VERIFY_NOT_EQUAL(x, FailValue) \
if ((x) == (FailValue)) \
{ \
	INSERT_DBGBREAK \
	throw std::runtime_error("Failed call to " #x " at " __FILE__ ":" S_(__LINE__) ", expected not " #FailValue); \
}

#define NOTIMPL do { INSERT_DBGBREAK \
	throw std::runtime_error(__FUNCTION__ " is not implemented!"); } while(0);

#include <wrl.h>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#ifdef NDEBUG
#define SE_ENSURE( expr ) \
		(!!(expr))	
#else
#define SE_ENSURE( expr ) \
		((!!(expr)) || \
		(_wassert(_CRT_WIDE(#expr), _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0))
#endif

#define SE_ENSURE_HRES( expr )\
		SE_ENSURE( SUCCEEDED(( expr )) )

#include <dxcapi.h>

#ifndef IMPLEMENT_SCOPED_ENUM_FLAGS
#define IMPLEMENT_SCOPED_ENUM_FLAGS(T) \
inline constexpr T operator|(T lhs, T rhs) \
{ \
	return T( \
		std::underlying_type<T>::type(lhs) \
		| std::underlying_type<T>::type(rhs)); \
} \
inline constexpr T operator&(T lhs, T rhs) \
{ \
	return T( \
		std::underlying_type<T>::type(lhs) \
		& std::underlying_type<T>::type(rhs)); \
}
#endif

// for use in RHIObject inheritance chains
#define GENERATE_RHI_OBJECT_BODY_NO_RHI() \
private: \
	std::atomic<int32_t> m_ref_counter = 0; \
public: \
	virtual void AddRef() override; \
	virtual void Release() override; \
private:

// use this macro to define RHIObject body
#define GENERATE_RHI_OBJECT_BODY() \
protected: \
	class VulkanRHI* m_rhi = nullptr; \
	GENERATE_RHI_OBJECT_BODY_NO_RHI()


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

#define IMPLEMENT_RHI_INTERFACE(InterfaceType, ImplementationType) \
template<> struct RHIImplClass<InterfaceType> \
{ \
	using Type = ImplementationType; \
};