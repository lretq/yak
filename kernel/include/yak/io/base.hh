#pragma once

#include <cstddef>
#include <string.h>

class Object;

class ClassInfo {
    public:
	const char *className = nullptr;
	const ClassInfo *superClass = nullptr;
	Object *(*createInstance)() = nullptr;
};

#define IO_OBJ_DECLARE_COMMON(className) \
    public:                              \
	static ClassInfo classInfo;      \
	constexpr className()            \
	{                                \
	}                                \
	virtual ~className()             \
	{                                \
	}

#define IO_OBJ_DECLARE_ROOT(className)   \
	IO_OBJ_DECLARE_COMMON(className) \
	virtual const ClassInfo *getClassInfo() const;

#define IO_OBJ_DECLARE(className)                               \
    public:                                                     \
	IO_OBJ_DECLARE_COMMON(className)                        \
	virtual const ClassInfo *getClassInfo() const override; \
	static Object *createInstance();

#define IO_OBJ_DEFINE_COMMON(_className, superClassInfo, instance) \
	const ClassInfo *_className::getClassInfo() const          \
	{                                                          \
		return &_className::classInfo;                     \
	}                                                          \
	ClassInfo _className::classInfo = {                        \
		.className = #_className,                          \
		.superClass = superClassInfo,                      \
		.createInstance = instance,                        \
	};

#define IO_OBJ_DEFINE_VIRTUAL(_className, superClass) \
	IO_OBJ_DEFINE_COMMON(_className, &superClass::classInfo, nullptr)

#define IO_OBJ_DEFINE(_className, superClass)                    \
	IO_OBJ_DEFINE_COMMON(_className, &superClass::classInfo, \
			     _className::createInstance)         \
	Object *_className::createInstance()                     \
	{                                                        \
		return new _className();                         \
	}

#define IO_OBJ_DEFINE_ROOT(className) \
	IO_OBJ_DEFINE_COMMON(className, nullptr, nullptr)

#define IO_OBJ_CREATE(className) ((className *)className::createInstance())

#define ALLOC_INIT(var, className)              \
	do {                                    \
		var = IO_OBJ_CREATE(className); \
		(var)->init();                  \
	} while (0)

class Object {
    public:
	IO_OBJ_DECLARE_ROOT(Object);

	bool isKindOf(const char *name) const
	{
		const ClassInfo *info = getClassInfo();
		while (info) {
			if (strcmp(info->className, name) == 0)
				return true;
			info = info->superClass;
		}
		return false;
	}

	bool isKindOf(ClassInfo *classInfo) const
	{
		return isKindOf(classInfo->className);
	}

	void retain()
	{
		__atomic_fetch_add(&refcnt_, 1, __ATOMIC_ACQUIRE);
	}

	void release()
	{
		if (__atomic_fetch_sub(&refcnt_, 1, __ATOMIC_RELEASE)) {
			// commit suicide
			deinit();
			delete this;
		}
	}

	virtual void init()
	{
		refcnt_ = 1;
	}

	virtual void deinit()
	{
	}

	virtual bool isEqual(Object *other) const;

	template <typename T> T *safe_cast()
	{
		return (this->isKindOf(&T::classInfo)) ?
			       static_cast<T *>(this) :
			       nullptr;
	}

    private:
	size_t refcnt_;
};
