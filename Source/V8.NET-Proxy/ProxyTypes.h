// V8 proxy exports header for the Dream Space internet development framework.
// This source is released under LGPL.

#include <exception>
#include <vector>

#if DEBUG
#include <algorithm>
#endif

#if (_MSC_PLATFORM_TOOLSET >= 110)
#include <mutex>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <cstddef>

//#include <include\v8stdint.h>
#include "Platform.h"

#if (_MSC_PLATFORM_TOOLSET < 110)
#define nullptr NULL
#endif

#if _WIN32 || _WIN64
#include <windows.h>
//#pragma comment(lib, "winmm.lib") // (required by V8 now)
//#include <oleauto.h>
#define ALLOC_MANAGED_MEM(size) GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT, size)
#define REALLOC_MANAGED_MEM(ptr, size) GlobalReAlloc(ptr, size, GMEM_MOVEABLE)
#define FREE_MANAGED_MEM(ptr) { GlobalFree(ptr); ptr = nullptr; }
//??#define ALLOC_MANAGED_STRING(size) CoTaskMemAlloc(GMEM_FIXED|GMEM_ZEROINIT, size)
//#define FREE_MARSHALLED_STRING(ptr) { CoTaskMemFree(ptr); ptr = nullptr; }
#define STDCALL __stdcall
#else
#include <glib.h>
#define ALLOC_MANAGED_MEM(size) malloc(size)
#define REALLOC_MANAGED_MEM(ptr, size) realloc(ptr, size)
#define FREE_MANAGED_MEM(ptr) free(ptr)
#define STDCALL 
#endif

//(make static instead) #define USING_V8_SHARED 1
#define V8_USE_UNSAFE_HANDLES 1 // (see https://groups.google.com/forum/#!topic/v8-users/oBE_DTpRC08)

#include "libplatform/libplatform.h"
#include "v8.h"

#include <functional>
#include <libplatform/libplatform.h>
//#include "Inspector.h"
//#include "v8_inspector_listener_impl.h"

#if _WIN32 || _WIN64
#pragma comment(lib, "v8_monolith.lib")
/*#pragma comment(lib, "v8_base_0.lib")
#pragma comment(lib, "v8_base_1.lib")
#pragma comment(lib, "v8_snapshot.lib")
#pragma comment(lib, "v8_nosnapshot.lib")
#pragma comment(lib, "v8_libbase.lib")
#pragma comment(lib, "v8_libplatform.lib")
#pragma comment(lib, "v8_libsampler.lib")
*/#pragma comment(lib, "third_party/icu/icuuc.lib")
#pragma comment(lib, "third_party/icu/icui18n.lib")
#pragma comment(lib, "src/inspector/inspector.lib")

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shlwapi.lib")
#endif

using namespace v8;

//#define EXPORT __declspec(dllexport)
#if defined(_MSC_VER__V8_NET)
    //  Microsoft 
    #define EXPORT __declspec(dllexport)
    #define IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
    //  GCC
    #define EXPORT __attribute__((visibility("default")))
    #define IMPORT
#else
    //  do nothing and hope for the best?
    #define EXPORT
    #define IMPORT
    #pragma warning Unknown dynamic link import/export semantics.
#endif

// ========================================================================================================================

// 
/**
 * Allows a handle to be copied to a persistent handle (outside normal
  * scopes), including between other CopyablePersistent values.
 *
 * This is a reference that is independent of any handle scope.  Where
 * a Local handle only lives as long as the HandleScope in which it was
 * allocated, a persisted handle remains valid until it is explicitly
 * disposed using Reset().
 *
 * A persistent handle contains a reference to a storage cell within
 * the V8 engine which holds an object value and which is updated by
 * the garbage collector whenever the object is moved.  A new storage
 * cell can be created using the constructor or Reset(), and
 * existing handles can be disposed using Reset().
 *
 */
template <class T> struct CopyablePersistent {
	v8::Persistent<T, CopyablePersistentTraits<T>> Value;

	CopyablePersistent() { }
	CopyablePersistent(CopyablePersistent &p) { Value = p.Value; }
	// Local<T> must get converted to a persistent type before it goes out of scope and gets disposed within the calling scope.
	CopyablePersistent(Local<T> h) { Value = v8::Persistent<T, CopyablePersistentTraits<T>>(Isolate::GetCurrent(), h); }
	~CopyablePersistent() { if (!Value.IsEmpty()) Value.Reset(); }

	// Just to support the = operator as well as the copy constructor.
	CopyablePersistent& operator= (const Local<T>& h) { Value = v8::Persistent<T, CopyablePersistentTraits<T>>(Isolate::GetCurrent(), h); return *this; }
	operator Local<T>() const { return Handle(); }

	T* operator ->() const { return *Handle(); }

	/* Returns the local handle for the persisted value.  Make sure to be in the handle scope before calling. */
	Local<T> Handle() const { return Local<T>::New(Isolate::GetCurrent(), Value); }
	bool IsEmpty() const { return Value.IsEmpty(); }
	bool IsWeak() const { return Value.IsWeak(); }
	bool IsNearDeath() const { return Value.IsNearDeath(); }
	bool IsIndependent() const { return Value.IsIndependent(); }
	void Reset() { return Value.Reset(); }
	void MarkIndependent() { return Value.MarkIndependent(); }
	void MarkPartiallyDependent() { return Value.MarkPartiallyDependent(); }

	template <class S> Local<S> As() { return Local<S>::Cast(Handle()); } //Handle().As<S>(); }
};

#define byte unsigned char
#define int32_t std::int32_t
#define int64_t std::int64_t
//#define vector std::vector
#define exception std::exception
#define runtime_error std::runtime_error
#define recursive_mutex std::recursive_mutex
#define lock_guard std::lock_guard

template <class S> S ToThrow(Maybe<S> valueM, std::string msg) 
{
	S value;
	if (!valueM.To(&value))
		throw runtime_error(msg);
	return value; 
}

template <class S> S ToThrow(Maybe<S> valueM) 
{
	return ToThrow(valueM, "Maybe is empty.");
}

template <class S> Local<S> ToLocalThrow(MaybeLocal<S> valueML, std::string msg) 
{
	Local<S> valueL;
	if (!valueML.ToLocal(&valueL))
		throw runtime_error(msg);
	return valueL; 
}

template <class S> Local<S> ToLocalThrow(MaybeLocal<S> valueML) 
{
	return ToLocalThrow(valueML, "MaybeLocal is empty.");
}


#define V8Undefined v8::Undefined(Isolate::GetCurrent())
#define V8Null v8::Null(Isolate::GetCurrent())
#define NewNumber(value) Number::New(Isolate::GetCurrent(), value)
#define NewInteger(value) Int32::New(Isolate::GetCurrent(), value)
#define NewBool(value) Boolean::New(Isolate::GetCurrent(), value)
#define NewSizedUString(str, len) String::NewFromTwoByte(Isolate::GetCurrent(), str, NewStringType::kNormal, len)
#define NewUString(str) String::NewFromTwoByte(Isolate::GetCurrent(), str, NewStringType::kNormal)
#define NewName(str) NewUString(str)
#define NewSizedString(str, len) String::NewFromUtf8(Isolate::GetCurrent(), str, NewStringType::kNormal, len)
#define NewString(str) String::NewFromUtf8(Isolate::GetCurrent(), str, NewStringType::kNormal)
#define NewPrivateString(str) Private::New(Isolate::GetCurrent(), ToLocalThrow(NewString(str)))
#define NewObject() Object::New(Isolate::GetCurrent())
#define NewDate(ctx, ms) ToLocalThrow(Date::New(ctx, ms))
#define NewArray(len) Array::New(Isolate::GetCurrent(), len)
#define NewObjectTemplate() ObjectTemplate::New(Isolate::GetCurrent())
#define NewFunctionTemplate(callback, data) FunctionTemplate::New(Isolate::GetCurrent(), callback, data)
#define NewExternal(ptr) External::New(Isolate::GetCurrent(), ptr)
#define ThrowException(value) Isolate::GetCurrent()->ThrowException(value)

//#define BEGIN_HANDLE_SCOPE(_this) \
//{ \
//    v8::Locker __lockScope(_this->_Isolate); \
//    v8::HandleScope __handleScope(_this->_Isolate);
//
//#define END_HANDLE_SCOPE \
//    __handleScope; /* (prevent non-usage warnings) */ \
//    __lockScope; \
//}

#define BEGIN_ISOLATE_SCOPE(engine) \
{ \
    v8::Locker __lockScope(engine->Isolate()); \
    v8::Isolate::Scope __isolateScope(engine->Isolate()); \
    v8::HandleScope __handleScope(Isolate::GetCurrent());

#define END_ISOLATE_SCOPE \
    __handleScope; \
    __isolateScope; \
    __lockScope; \
}

#define BEGIN_CONTEXT_SCOPE(engine) \
{ \
    v8::Context::Scope __contextScope(engine->Context());

#define END_CONTEXT_SCOPE \
    __contextScope; \
}

// ========================================================================================================================

class ProxyBase;
class ObjectTemplateProxy;
class FunctionTemplateProxy;
class V8EngineProxy;

struct HandleProxy;
struct HandleValue;

// Get rid of some linker warnings regarding certain V8 object references.
// (see https://groups.google.com/forum/?fromgroups=#!topic/v8-users/OuZPd0n-oRg)
namespace v8 {
	namespace internal {
		class Object { };
		class Isolate { };
	}
}

// ========================================================================================================================
// Utility functions.

///**
//* Converts a given V8 string into a uint16_t* string using ALLOC_MANAGED_MEM().
//* The string is expected to be freed by calling FREE_MANAGED_MEM(), or within a managed assembly.
//*/
//??uint16_t* V8StringToUInt16(v8::String* str);

// ========================================================================================================================

// Proxy object type enums.
enum  TProxyObjectType
{
	Undefined,
	ObjectTemplateProxyClass,
	FunctionTemplateProxyClass,
	V8EngineProxyClass,
	HandleProxyClass,
	ContextProxyClass
};

// ========================================================================================================================

#pragma pack(push, 1)
// The proxy base class helps to identify objects when references are passed between native and managed mode.
class ProxyBase
{
public:
	TProxyObjectType GetType() { return Type; }

protected:
	TProxyObjectType Type;

	ProxyBase(TProxyObjectType type) :Type(type) { }
};
#pragma pack(pop)

// ========================================================================================================================

//??#pragma enum(4)
// Types supported by HandleProxy.
enum JSValueType : int32_t
{
	JSV_ExecutionTerminated = -4, // 'TerminateExecution()' was called.
	JSV_ExecutionError = -3, // An error has occurred while attempting to execute the compiled script.
	JSV_CompilerError = -2, // An error has occurred compiling the script (usually a syntax error).
	JSV_InternalError = -1, // An internal error has occurred (before or after script execution).
	JSV_Uninitialized = 0, // The value type has yet to be determined.
	JSV_Undefined, // Value is the JavaScript 'undefined' value.
	JSV_Script, // The handle represents a compiled script.
	JSV_Null, // Value is the JavaScript 'null' value.
	JSV_Bool, // The value is a JavaScript Boolean, as supported within JavaScript for true/false conditions.
	JSV_BoolObject, // The value is a JavaScript Boolean object (object reference), as supported within JavaScript when executing "new Boolean()".
	JSV_Int32, // The value is a 32-bit JavaScript integer, as supported within JavaScript for bit operations.
	JSV_Number, // The value is a JavaScript 64-bit number.
	JSV_NumberObject, // The value is a JavaScript 64-bit number object (object reference), as supported within JavaScript when executing "new Number()".
	JSV_String, // The value is a JavaScript UTF16 string.
	JSV_StringObject, // The value is a JavaScript string object (object reference), as supported within JavaScript when executing "new String()".
	JSV_Object, // The value is a JavaScript object reference (i.e. not a primitive value).
	JSV_Function, // The value is a reference to a JavaScript function (object reference).
	JSV_Date, // The date value is the number of milliseconds since epoch [1970-01-01 00:00:00 UTC+00] (a double value stored in 'Number').
	JSV_Array, // The value proxy represents a JavaScript array of various values.
	JSV_RegExp, // The value is a reference to a JavaScript RegEx object (object reference).

	// (when updating, don't forget to update V8EngineProxy.Enums.cs also!)
};
//??#pragma enum(pop)

// ========================================================================================================================

#pragma pack(push, 1)
// While "HandleProxy" tracks values/objects by handle, this type helps to marshal the underlying values to the managed side when needed.
struct HandleValue
{
	union
	{
		bool V8Boolean; // JavaScript Boolean.
		int64_t V8Integer; // JavaScript 32-bit integer, but this is 64-bit to maintain consistent union size!!! (double is 64-bit in x64, and 32-bit in x86)
		double V8Number; // JavaScript number (double [32/64-bit float]).
	};

	union
	{
		uint16_t *V8String; // JavaScript string.
		int64_t _V8String; // (to keep pointer sizes consistent between 32 and 64 bit systems)
	};

	HandleValue();

	~HandleValue();

	void Dispose();
};
#pragma pack(pop)

// ========================================================================================================================

#pragma pack(push, 1)
// Provides a mechanism by which to keep track of V8 objects associated with managed side objects.
struct HandleProxy : ProxyBase // TODO: Make a separate VALUE based handle proxy and use this for templates also.
{
private:
	int32_t _ID; // The ID of this handle (handles are cached/recycled and not destroyed). The ID is also used on the managed side.

	int32_t _ObjectID; // The ID (index) of any associated managed object in V8.NET.  This is -1 by default, and is only update when 'GetManagedObjectID()' is called.
	int32_t _CLRTypeID; // A special ID to an array of registered CLR types that this object represents, otherwise this is -1.

	JSValueType _Type; // Note: a 32-bit type value (the managed code will expect 4 bytes).

	HandleValue _Value; // The value is only valid when 'UpdateValue()' is called. Note: sizeof(double) + sizeof(uint16_t*)

	int32_t _ManagedReference; // This is set to 1 for InternalHandle references, and 2 if there is a managed side reference to this proxy object that is responsible for it.

	// When a handle is created this is 0.  When the native side is ready to dispose the handle a callback is triggered.  If the handle does not have a managed side object
	// then the handle is disposed, otherwise it survives the V8 GC process, and this field is set to 1.  If the managed side no longer has references to the handle then
	// this is set to 2 and 'MakeWeak()' is called on the native handle. If this value is negative then it is queued for that stage.
	int32_t _Disposed;  // (flags: 0 = handle is in use, 1 = disposed, 2 = managed side is done with it, 3 - VIRTUALLY disposed [cached on native side for reuse]), 4 = queued for making weak (managed side call), 8 = 'weak' removal in progress, 16 = queued for disposal.
	//? Old Meaning: (0: handle is in use, 1: managed disposing in progress, 2: handle was made weak - managed side is done with it, 3: VIRTUALLY disposed [cached on native side for reuse])

	int32_t _EngineID;

	union
	{
		V8EngineProxy* _EngineProxy;
		int64_t __EngineProxy; // (to keep pointer sizes consistent between 32 and 64 bit systems)
	};

	CopyablePersistent<Value> _Handle; // Reference to a JavaScript object (persisted handle for future reference - WARNING: Must be explicitly released when no longer needed!).
	CopyablePersistent<v8::Script> _Script; // (references a script handle [instead of a value one])

	//static void _DisposeCallback(const WeakCallbackInfo<HandleProxy>& data);
	static void _RevivableCallback(const WeakCallbackInfo<HandleProxy>& data);

protected:

	HandleProxy(V8EngineProxy* engineProxy, int id);
	~HandleProxy();

	HandleProxy* Initialize(v8::Handle<Value> handle);
	HandleProxy* SetHandle(v8::Handle<Value> handle);
	HandleProxy* SetHandle(v8::Handle<v8::Script> handle);
	HandleProxy* SetDate(double ms);

	void _ClearHandleValue();
	bool _Dispose(bool registerDisposal);

public:

	int32_t SetManagedObjectID(int32_t id);
	int GetManagedObjectID();
	static int GetManagedObjectID(v8::Handle<Value> h);

	bool IsError() { return _Type < 0; }

	bool IsScript() { return _Type == JSV_Script; }

	// Disposes of the handle that is wrapped by this proxy instance.
	// This call always succeeds if disposal has been started by setting '_Disposed' to 1 or 2.
	bool Dispose();

	// The handle is currently in use.
	bool IsInUse() { return (_Disposed & 19) == 0; }
	// The managed side is done with the handle.
	bool IsDisposeReadyManagedSide() { return (_Disposed & 2) > 0 || (_Disposed & 4) > 0 || _ManagedReference < 2; }
	// The handle is queued for disposal.
	bool IsDisposing() { return (_Disposed & 16) > 0; }
	// The handle is disposed and cached.
	bool IsDisposed() { return (_Disposed & 1) > 0; }

	// Attempts to dispose a handle passed in from the managed side.
	// By default, handle proxies returned from callbacks to the managed side must be disposed, just like arguments.  The
	// managed side is responsible for keeping them alive if needed.
	// Note: This also includes handles passed in as arguments, such as when setting properties on objects.
	bool TryDispose();

	// (expected to be called by a managed garbage collection thread [of some sort, but not the main thread])
   //? void _ManagedGCCallback();

	// Attempts to delete the handle, which will succeed only if the engine is gone, otherwise Dispose() is called)
	//?void Delete();

	V8EngineProxy* EngineProxy(); // Returns the associated engine, or null if the engine was disposed.
	int32_t EngineID() { return _EngineID; }

	Local<Value> Handle();
	Local<Script> Script();

	void MakeWeak();
	void MakeStrong();

	void UpdateValue();

	friend V8EngineProxy;
	friend ObjectTemplateProxy;
	friend FunctionTemplateProxy;
};
#pragma pack(pop)

// ========================================================================================================================

/**
* Usually allocated on the stack before being passed to a managed call-back when triggered by script access.
*/
#pragma pack(push, 1)
struct ManagedAccessorInfo
{
private:

	ObjectTemplateProxy* _ObjectProxy; // (this is AccessorInfo::Holder() related, or nullptr for non-ObjectTemplate created objects) 
	int32_t _ObjectID; // If set (>=0), then this instance is a new JavaScript object, created from a template, and associated with a managed object. Default is -1.
	//?? (not sure if the ID is needed here)

public:

	Local<Value> Data;
	Local<Object> This;

	ManagedAccessorInfo(ObjectTemplateProxy* objectProxy, int32_t managedObjectID, const PropertyCallbackInfo<Value>& info)
		: _ObjectProxy(objectProxy), _ObjectID(managedObjectID)
	{
		Data = info.Data();
		This = info.This();
	}
	ManagedAccessorInfo(ObjectTemplateProxy* objectProxy, int32_t managedObjectID, const PropertyCallbackInfo<Integer>& info)
		: _ObjectProxy(objectProxy), _ObjectID(managedObjectID)
	{
		Data = info.Data();
		This = info.This();
	}
	ManagedAccessorInfo(ObjectTemplateProxy* objectProxy, int32_t managedObjectID, const PropertyCallbackInfo<Boolean>& info)
		: _ObjectProxy(objectProxy), _ObjectID(managedObjectID)
	{
		Data = info.Data();
		This = info.This();
	}
	ManagedAccessorInfo(ObjectTemplateProxy* objectProxy, int32_t managedObjectID, const PropertyCallbackInfo<Array>& info)
		: _ObjectProxy(objectProxy), _ObjectID(managedObjectID)
	{
		Data = info.Data();
		This = info.This();
	}
};
#pragma pack(pop)

// ========================================================================================================================
// Managed call-back delegate types.

typedef void (STDCALL *CallbackAction)();

/**
* NamedProperty[Getter|Setter] are used as interceptors on object.
* See ObjectTemplate::SetNamedPropertyHandler.
*/
typedef HandleProxy* (STDCALL *ManagedNamedPropertyGetter)(uint16_t* propertyName, const ManagedAccessorInfo& info);

/**
* Returns the value if the setter intercepts the request.
* Otherwise, returns an empty handle.
*/
typedef HandleProxy* (STDCALL *ManagedNamedPropertySetter)(uint16_t* propertyName, HandleProxy* value, const ManagedAccessorInfo& info);

/**
* Returns a non-empty value (>=0) if the interceptor intercepts the request.
* The result is an integer encoding property attributes (like v8::None,
* v8::DontEnum, etc.)
*/
typedef PropertyAttribute(STDCALL *ManagedNamedPropertyQuery)(uint16_t* propertyName, const ManagedAccessorInfo& info);

/**
* Returns a value indicating if the deleter intercepts the request.
* The return value is true (>0) if the property could be deleted and false (0)
* otherwise.
*/
typedef int (STDCALL *ManagedNamedPropertyDeleter)(uint16_t* propertyName, const ManagedAccessorInfo& info);

/**
* Returns an array containing the names of the properties the named
* property getter intercepts.
*/
typedef HandleProxy* (STDCALL *ManagedNamedPropertyEnumerator)(const ManagedAccessorInfo& info);

// ------------------------------------------------------------------------------------------------------------------------
/**
* Returns the value of the property if the getter intercepts __stdcall
*/
typedef HandleProxy* (STDCALL *ManagedIndexedPropertyGetter)(uint32_t index, const ManagedAccessorInfo& info);

/**
* Returns the value if the setter intercepts the request.
* Otherwise, returns an empty handle.
*/
typedef HandleProxy* (STDCALL *ManagedIndexedPropertySetter)(uint32_t index, HandleProxy* value, const ManagedAccessorInfo& info);

/**
* Returns a non-empty handle if the interceptor intercepts the request.
* The result is an integer encoding property attributes.
*/
typedef PropertyAttribute(STDCALL *ManagedIndexedPropertyQuery)(uint32_t index, const ManagedAccessorInfo& info);

/**
* Returns a non-empty handle if the deleter intercepts the request.
* The return value is true if the property could be deleted and false
* otherwise.
*/
typedef int (STDCALL *ManagedIndexedPropertyDeleter)(uint32_t index, const ManagedAccessorInfo& info);

/**
* Returns an array containing the indices of the properties the
* indexed property getter intercepts.
*/
typedef HandleProxy* (STDCALL *ManagedIndexedPropertyEnumerator)(const ManagedAccessorInfo& info);

// ------------------------------------------------------------------------------------------------------------------------

/**
* Intercepts requests on objects with getters applied.
*/
typedef HandleProxy* (STDCALL *ManagedAccessorGetter)(HandleProxy *_this, uint16_t* propertyName);

/**
* Intercepts requests on objects with setters applied.
The return is always undefined, unless an error occurs.
*/
typedef HandleProxy* (STDCALL *ManagedAccessorSetter)(HandleProxy *_this, uint16_t* propertyName, HandleProxy* value);

// ------------------------------------------------------------------------------------------------------------------------

// A managed call-back that is triggered when a native object has no more references.
// When the managed side is notified of no more JavaScript/V8 references, then the associated strong-reference on the managed side is cleared to allow the
// managed weak reference to track the managed object.  Persisted object handles will be disposed when the managed objects are finalized.
typedef bool (STDCALL *ManagedV8GarbageCollectionRequestCallback)(HandleProxy *hProxy);
//typedef bool (STDCALL *ManagedV8GarbageCollectionRequestCallback)(ContextProxy* nativeContext, HandleProxy *hProxy); // TODO [shlomo] for moving _Objects to context

// ------------------------------------------------------------------------------------------------------------------------

typedef HandleProxy* (STDCALL *ManagedJSFunctionCallback)(int32_t managedObjectID, bool isConstructCall, HandleProxy *_this, HandleProxy** args, uint32_t argCount);

// ========================================================================================================================

/**
  * A proxy class to encapsulate the call-back methods needed to resolve properties for representing a managed object.
  */
#pragma pack(push, 1)
class ObjectTemplateProxy : ProxyBase
{
protected:

	V8EngineProxy* _EngineProxy = nullptr;
	int32_t _EngineID;
	int32_t _ObjectID; // ObjectTemplate will have a "shared" object ID for use with associating accessors (see ObjecctTemplate.SetAccessor() in V8.Net).
	bool _WasUsed = false;
	CopyablePersistent<ObjectTemplate> _ObjectTemplate;

	ManagedNamedPropertyGetter NamedPropertyGetter = nullptr;
	ManagedNamedPropertySetter NamedPropertySetter = nullptr;
	ManagedNamedPropertyQuery NamedPropertyQuery = nullptr;
	ManagedNamedPropertyDeleter NamedPropertyDeleter = nullptr;
	ManagedNamedPropertyEnumerator NamedPropertyEnumerator = nullptr;

	ManagedIndexedPropertyGetter IndexedPropertyGetter = nullptr;
	ManagedIndexedPropertySetter IndexedPropertySetter = nullptr;
	ManagedIndexedPropertyQuery IndexedPropertyQuery = nullptr;
	ManagedIndexedPropertyDeleter IndexedPropertyDeleter = nullptr;
	ManagedIndexedPropertyEnumerator IndexedPropertyEnumerator = nullptr;

	ManagedJSFunctionCallback _ManagedCallback = nullptr; // (allows calling the objects created from this template like functions)

public:

	// Called when created by V8EngineProxy.
	ObjectTemplateProxy(V8EngineProxy* engineProxy);

	// Called by FunctionTemplateProxy to create a wrapper for the existing templates (auto generated with the FunctionTemplate instance).
	ObjectTemplateProxy(V8EngineProxy* engineProxy, Local<ObjectTemplate> objectTemplate);

	~ObjectTemplateProxy();

	V8EngineProxy* EngineProxy(); // Returns the associated engine, or null if the engine was disposed.
	int32_t EngineID() { return _EngineID; }

	void RegisterNamedPropertyHandlers(
		ManagedNamedPropertyGetter getter,
		ManagedNamedPropertySetter setter,
		ManagedNamedPropertyQuery query,
		ManagedNamedPropertyDeleter deleter,
		ManagedNamedPropertyEnumerator enumerator);

	void RegisterIndexedPropertyHandlers(
		ManagedIndexedPropertyGetter getter,
		ManagedIndexedPropertySetter setter,
		ManagedIndexedPropertyQuery query,
		ManagedIndexedPropertyDeleter deleter,
		ManagedIndexedPropertyEnumerator enumerator);

	void SetCallAsFunctionHandler(ManagedJSFunctionCallback callback);

	void UnregisterNamedPropertyHandlers();
	void UnregisterIndexedPropertyHandlers();

	static void GetProperty(Local<Name> hName, const PropertyCallbackInfo<Value>& info);
	static void SetProperty(Local<Name> hName, Local<Value> value, const PropertyCallbackInfo<Value>& info);
	static void GetPropertyAttributes(Local<Name> hName, const PropertyCallbackInfo<Integer>& info);
	static void DeleteProperty(Local<Name> hName, const PropertyCallbackInfo<Boolean>& info);
	static void GetPropertyNames(const PropertyCallbackInfo<Array>& info);

	static void GetProperty(uint32_t index, const PropertyCallbackInfo<Value>& info);
	static void SetProperty(uint32_t index, Local<Value> hValue, const PropertyCallbackInfo<Value>& info);
	static void GetPropertyAttributes(uint32_t index, const PropertyCallbackInfo<Integer>& info);
	static void DeleteProperty(uint32_t index, const PropertyCallbackInfo<Boolean>& info);
	static void GetPropertyIndices(const PropertyCallbackInfo<Array>& info);

	static void AccessorGetterCallbackProxy(Local<Name> property, const PropertyCallbackInfo<Value>& info);
	static void AccessorSetterCallbackProxy(Local<Name> property, Local<Value> value, const PropertyCallbackInfo<void>& info);

	HandleProxy* CreateObject(int32_t managedObjectID);

	void SetAccessor(int32_t managedObjectID, const uint16_t *name,
		ManagedAccessorGetter getter, ManagedAccessorSetter setter,
		v8::AccessControl access, v8::PropertyAttribute attributes);

	void Set(const uint16_t *name, HandleProxy *value, v8::PropertyAttribute attributes);

	friend V8EngineProxy;
	friend FunctionTemplateProxy;
};
#pragma pack(pop)

// ========================================================================================================================

/**
* A proxy class to encapsulate the call-back methods needed to resolve properties for representing a managed object.
*/
#pragma pack(push, 1)
class FunctionTemplateProxy : ProxyBase
{
protected:

	V8EngineProxy* _EngineProxy;
	int32_t _EngineID;
	CopyablePersistent<FunctionTemplate> _FunctionTemplate;
	ObjectTemplateProxy* _InstanceTemplate;
	ObjectTemplateProxy* _PrototypeTemplate;

	ManagedJSFunctionCallback _ManagedCallback;

public:

	FunctionTemplateProxy(V8EngineProxy* engineProxy, uint16_t* className, ManagedJSFunctionCallback managedCallback = nullptr);
	~FunctionTemplateProxy();

	V8EngineProxy* EngineProxy(); // Returns the associated engine, or null if the engine was disposed.
	int32_t EngineID() { return _EngineID; }

	void SetManagedCallback(ManagedJSFunctionCallback managedCallback);

	static void InvocationCallbackProxy(const FunctionCallbackInfo<Value>& args);

	ObjectTemplateProxy* GetInstanceTemplateProxy();
	ObjectTemplateProxy* GetPrototypeTemplateProxy();

	HandleProxy* GetFunction();
	//??HandleProxy* GetPrototype(int32_t managedObjectID);

	HandleProxy* CreateInstance(int32_t managedObjectID, int32_t argCount, HandleProxy** args);

	void Set(const uint16_t *name, HandleProxy *value, v8::PropertyAttribute attributes);

	friend V8EngineProxy;
	friend ObjectTemplateProxy;
};
#pragma pack(pop)

// ========================================================================================================================

typedef void DebugMessageDispatcher();

// ========================================================================================================================

struct _StringItem
{
	V8EngineProxy *Engine;
	uint16_t* String;
	size_t Length;

	_StringItem();
	_StringItem(V8EngineProxy *engine, size_t length);
	_StringItem(V8EngineProxy *engine, v8::String* str);

	void Free(); // Releases the string memory.

	_StringItem ResizeIfNeeded(size_t newLength); // Resizes the string to support the specified new length.
	void Dispose(); // Disposes of the string if one exists.
	void Clear(); // Clears the fields without disposing anything.
};

// ========================================================================================================================

/**
* A proxy class to encapsulate a JavaScript execution context.
*/
#pragma pack(push, 1)
class ContextProxy : ProxyBase
{
protected:

	V8EngineProxy* _EngineProxy;
	int32_t _EngineID;
	CopyablePersistent<v8::Context> _Context;

public:

	ContextProxy(V8EngineProxy* engineProxy, CopyablePersistent<Context> context);
	~ContextProxy();

	V8EngineProxy* EngineProxy() { return _EngineProxy; }
	int32_t EngineID() { return _EngineID; }

	friend V8EngineProxy;
};
#pragma pack(pop)

// ========================================================================================================================

class V8EngineProxy : ProxyBase
{
protected:

	int32_t _EngineID; // NOTE: This MUST be the FIRST field (expected by the managed site).

	// Keeps track of engines being disposed to prevent managed handles from trying to dispose of invalidated V8 handles!
	// Why? If not, then we need to keep track of handles in some form of collection, list, or array. Since handles are a core part of values being handed
	// around, this would greatly impact performance. Since it is assumed that engines will not be created and disposed in large numbers, if at all, a
	// record of disposed engines is kept so handles can quickly check if they are ok to be disposed (note: managed handles are disposed on a GC thread!).
	static std::vector<bool> _DisposedEngines;
	static int32_t _NextEngineID;

	int32_t _NextNonTemplateObjectID;

	std::unique_ptr<v8::Platform> _Platform;
	Isolate* _Isolate;
	//?ObjectTemplateProxy* _GlobalObjectTemplateProxy; // (for working with the managed side regarding the global scope)
	CopyablePersistent<v8::Context> _Context;
	CopyablePersistent<v8::Object> _GlobalObject; // (taken from the context)
	ManagedV8GarbageCollectionRequestCallback _ManagedV8GarbageCollectionRequestCallback;

	//const static int32_t _InspectorPort = 9000;
	//std::unique_ptr<Inspector> _Inspector = nullptr;

	std::vector<_StringItem> _Strings; // An array (cache) of string buffers to reuse when marshalling strings.

	std::vector<HandleProxy*> _Handles; // An array of all allocated handles for this engine proxy.
	std::vector<HandleProxy*> _HandlesPendingDisposal; // An array of handles for this engine proxy that are ready to be disposed.
	std::vector<int> _DisposedHandles; // An array of handles (by ID [index]) that have been disposed. The managed GC thread uses this, so beware!
	std::mutex _DisposingHandleMutex; // A mutex used to prevent access to the handle disposal queue system as a "critical section".  NO ACCESS TO THE V8 ENGINE IS ALLOWED FOR MANAGED GARBAGE COLLECTION IN THIS CRITICAL SECTION.
	recursive_mutex _HandleSystemMutex; // A mutex used to prevent access to the handle system as a "critical section".  NO ACCESS TO THE V8 ENGINE IS ALLOWED FOR MANAGED GARBAGE COLLECTION IN THIS CRITICAL SECTION.

	std::vector<HandleProxy*> _HandlesToBeMadeWeak;
	recursive_mutex _MakeWeakQueueMutex;
	std::vector<HandleProxy*> _HandlesToBeMadeStrong;
	recursive_mutex _MakeStrongQueueMutex;

	std::vector<HandleProxy*> _Objects; // An array of handle references by object ID. This allows pulling an already existing proxy handle for an object without having to allocate a new one.

	bool _IsExecutingScript; // True if the engine is executing a script.  This is used abort entering a locker on idle notifications while scripts are running.
	int _InCallbackScope; // >0 if currently in a scope that is/will call back to the manage side. This helps to notify when a callback to the managed side causes another call back into the engine.
	bool _IsTerminatingScript; // True if the engine was asked to terminate a script.  This is used to detect when a script is aborted.

public:

	Isolate* Isolate();
	Handle<Context> Context();

	V8EngineProxy(bool enableDebugging, DebugMessageDispatcher* debugMessageDispatcher, int debugPort);
	~V8EngineProxy();

	static Local<String> GetErrorMessage(Local<v8::Context> ctx, TryCatch &tryCatch);

	// Returns the next object ID for objects that do NOT have a corresponding object.  These objects still need an ID, and are given values less than -1.
	int32_t GetNextNonTemplateObjectID()
	{
		return _NextNonTemplateObjectID--;
	}

	// Gets or allocates a string buffer from within the cached strings array.
	_StringItem GetNativeString(v8::String* str);

	// Disposes a string returned via 'GetNativeString()'.
	void DisposeNativeString(_StringItem &item);

	// Gets an available handle proxy, or creates a new one, for the specified handle.
	HandleProxy* GetHandleProxy(Handle<Value> handle);

	// Queue a handle for disposal later.  This is typically done when the engine is busy running a script and another call is made
	// (possibly by the GC finalizer) to dispose a handle.
	void QueueHandleDisposal(HandleProxy *handleProxy);

	// Registers the handle proxy as disposed for recycling.
	void DisposeHandleProxy(HandleProxy *handleProxy);

	// Puts a handle proxy into a queue to be made weak via 'GetHandleProxy()' - which may be required during a long script execution.
	void QueueMakeWeak(HandleProxy *handleProxy);
	// Puts a handle proxy into a queue to be made strong via 'GetHandleProxy()' - which may be required during a long script execution.
	void QueueMakeStrong(HandleProxy *handleProxy);

	void ProcessHandleQueues(int loops = 1); // (must be called internally, NEVER externally [i.e. from managed side])

	// Registers a request to dispose a handle proxy for recycling.
	// WARNING: This is expected to be called by the GC to flag handles for disposal.
	//??void RequestDisposeHandleProxy(HandleProxy *handleProxy);

	void  RegisterGCCallback(ManagedV8GarbageCollectionRequestCallback managedV8GarbageCollectionRequestCallback);

	static bool IsDisposed(int32_t engineID);

	// True if the engine is executing a script.  This is used abort entering a locker on idle notifications, or to single when to queue a request, such as disposing handles from the managed side while script execution is in progress.
	bool IsExecutingScript();

	ContextProxy* CreateContext(ObjectTemplateProxy* templateProxy);
	HandleProxy* SetContext(ContextProxy *context); // (returns the global object after setting the context)
	ContextProxy* GetContext(); // (returns the current context as a proxy object for the CLR side)

	ObjectTemplateProxy* CreateObjectTemplate();

	FunctionTemplateProxy* CreateFunctionTemplate(uint16_t *className, ManagedJSFunctionCallback callback);

	HandleProxy* Execute(const uint16_t* script, uint16_t* sourceName);
	HandleProxy* Execute(Handle<Script> script);
	HandleProxy* Compile(const uint16_t* script, uint16_t* sourceName);

	void TerminateExecution();

	HandleProxy* Call(HandleProxy *subject, const uint16_t *functionName, HandleProxy *_this, uint16_t argCount, HandleProxy** args);

	HandleProxy* CreateNumber(double num);
	HandleProxy* CreateInteger(int32_t num);
	HandleProxy* CreateBoolean(bool b);
	HandleProxy* CreateString(const uint16_t* str);
	HandleProxy* CreateError(const char* message, JSValueType errorType);
	HandleProxy* CreateError(const uint16_t* message, JSValueType errorType);
	HandleProxy* CreateDate(double ms);
	HandleProxy* CreateArray(HandleProxy** items, uint16_t length);
	HandleProxy* CreateArray(uint16_t** items, uint16_t length);
	HandleProxy* CreateObject(int32_t managedObjectID);
	HandleProxy* CreateNullValue();

	Local<Private> CreatePrivateString(const char* data);
	void SetObjectPrivateValue(Local<Object> obj, const char* name, Local<Value> value);
	Local<Value> GetObjectPrivateValue(Local<Object> obj, const char* name);

	friend HandleProxy;
	friend ObjectTemplateProxy;
	friend FunctionTemplateProxy;
	friend ContextProxy;
};

// ========================================================================================================================

extern "C"
{
	EXPORT void STDCALL ConnectObject(HandleProxy *handleProxy, int32_t managedObjectID, void* templateProxy);
}
