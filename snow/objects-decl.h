#ifndef SN_BEGIN_CLASS
#define SN_BEGIN_CLASS(NAME)
#endif
#ifndef SN_BEGIN_THIN_CLASS
#define SN_BEGIN_THIN_CLASS(NAME)
#endif
#ifndef SN_END_CLASS
#define SN_END_CLASS()
#endif
#ifndef SN_BEGIN_THIN_CLASSES
#define SN_BEGIN_THIN_CLASSES()
#endif
#ifndef SN_END_THIN_CLASSES
#define SN_END_THIN_CLASSES()
#endif
#ifndef SN_BEGIN_CLASSES
#define SN_BEGIN_CLASSES()
#endif
#ifndef SN_END_CLASSES
#define SN_END_CLASSES()
#endif
#ifndef SN_BEGIN_IMMEDIATE_TYPES
#define SN_BEGIN_IMMEDIATE_TYPES()
#endif
#ifndef SN_END_IMMEDIATE_TYPES
#define SN_END_IMMEDIATE_TYPES()
#endif
#ifndef SN_DECLARE_IMMEDIATE_TYPE
#define SN_DECLARE_IMMEDIATE_TYPE(NAME)
#endif
#ifndef SN_MEMBER_DATA
#define SN_MEMBER_DATA(TYPE, NAME)
#endif
#ifndef SN_MEMBER_ROOT
#define SN_MEMBER_ROOT(TYPE, NAME)
#endif
#ifndef SN_MEMBER_ROOT_ARRAY
#define SN_MEMBER_ROOT_ARRAY(TYPE, NAME, SIZE_MEMBER)
#endif

SN_BEGIN_IMMEDIATE_TYPES()
SN_DECLARE_IMMEDIATE_TYPE(SnInteger)
SN_DECLARE_IMMEDIATE_TYPE(SnNil)
SN_DECLARE_IMMEDIATE_TYPE(SnBoolean)
SN_DECLARE_IMMEDIATE_TYPE(SnSymbol)
SN_DECLARE_IMMEDIATE_TYPE(SnFloat)
SN_END_IMMEDIATE_TYPES()

SN_BEGIN_THIN_CLASSES()

SN_BEGIN_THIN_CLASS(SnObject) // SnObject is "thin" since it itself doesn't derive from SnObject
	SN_MEMBER_DATA(byte, flags)
	// Remember to update gc.c / gc_mark_object when this struct changes!
	SN_MEMBER_ROOT(struct SnObject*, prototype)
	SN_MEMBER_ROOT(struct SnMap*, members)
	SN_MEMBER_ROOT(struct SnArray*, property_names)
	SN_MEMBER_ROOT(struct SnArray*, property_data)
	SN_MEMBER_ROOT(struct SnArray*, included_modules)
SN_END_CLASS()

SN_BEGIN_THIN_CLASS(SnContinuation)
	SN_MEMBER_DATA(SnFunctionPtr, function)
	SN_MEMBER_ROOT(struct SnContext*, context)
	SN_MEMBER_ROOT(VALUE, return_val)
	SN_MEMBER_ROOT(struct SnContinuation*, return_to)
	SN_MEMBER_ROOT(struct SnContinuation*, please_clean)
	SN_MEMBER_DATA(struct SnContinuationInternal*, internal)
SN_END_CLASS()

SN_BEGIN_THIN_CLASS(SnContext)
	SN_MEMBER_ROOT(struct SnFunction*, function)
	SN_MEMBER_ROOT(VALUE, self)
	SN_MEMBER_ROOT(struct SnArray*, locals)
	SN_MEMBER_ROOT(struct SnArguments*, args)
SN_END_CLASS()

SN_BEGIN_THIN_CLASS(SnArguments)
	SN_MEMBER_ROOT(struct SnArray*, names)
	SN_MEMBER_ROOT(struct SnArray*, data)
SN_END_CLASS()

SN_BEGIN_THIN_CLASS(SnFunctionDescription)
	SN_MEMBER_DATA(SnFunctionPtr, func)
	SN_MEMBER_DATA(SnSymbol, name)
	SN_MEMBER_ROOT(struct SnArray*, defined_locals)
	SN_MEMBER_ROOT(struct SnArray*, argument_names)
	SN_MEMBER_ROOT(struct SnAstNode*, ast)
	SN_MEMBER_DATA(uint16_t, num_variable_reference_descriptions)
	SN_MEMBER_ROOT_ARRAY(struct SnVariableReferenceDescription*, variable_reference_descriptions, num_variable_reference_descriptions)
SN_END_CLASS()

SN_BEGIN_THIN_CLASS(SnString)
	SN_MEMBER_DATA(char*, data)
	SN_MEMBER_DATA(uint32_t, size)   // bytes
	SN_MEMBER_DATA(uint32_t, length) // characters
SN_END_CLASS()

SN_BEGIN_THIN_CLASS(SnArray)
	SN_MEMBER_ROOT_ARRAY(VALUE*, data, size)
	SN_MEMBER_DATA(uint32_t, size)
	SN_MEMBER_DATA(uint32_t, alloc_size)
	SN_MEMBER_DATA(struct SnRWLock*, lock)
SN_END_CLASS()

SN_BEGIN_THIN_CLASS(SnMap)
	SN_MEMBER_ROOT_ARRAY(VALUE*, data, size)
	SN_MEMBER_DATA(uint32_t, size)
	SN_MEMBER_DATA(uint32_t, alloc_size)
	SN_MEMBER_DATA(SnCompareFunc, compare)
	SN_MEMBER_DATA(struct SnRWLock*, lock)
SN_END_CLASS()

SN_BEGIN_THIN_CLASS(SnCodegen)
	SN_MEMBER_ROOT(struct SnCodegen*, parent)
	SN_MEMBER_ROOT(struct SnFunctionDescription*, result)
	SN_MEMBER_ROOT(struct SnArray*, variable_reference_names)
	SN_MEMBER_ROOT(struct SnAstNode*, root)
	SN_MEMBER_ROOT(struct SnLinkBuffer*, buffer)
	SN_MEMBER_DATA(struct SnCodegenInternal*, internal)
SN_END_CLASS()

SN_BEGIN_THIN_CLASS(SnPointer)
	SN_MEMBER_DATA(void*, ptr)
	SN_MEMBER_DATA(SnPointerFreeFunc, free_func)
SN_END_CLASS()

SN_BEGIN_THIN_CLASS(SnAstNode)
	SN_MEMBER_DATA(SnAstNodeTypes, node_type)
	SN_MEMBER_ROOT(VALUE, child0)
	SN_MEMBER_ROOT(VALUE, child1)
	SN_MEMBER_ROOT(VALUE, child2)
	SN_MEMBER_ROOT(VALUE, child3)
SN_END_CLASS()

SN_BEGIN_THIN_CLASS(SnDeferredTask)
	SN_MEMBER_ROOT(VALUE, closure)
	SN_MEMBER_ROOT(VALUE, result)
	SN_MEMBER_DATA(void*, internal)
	SN_MEMBER_DATA(struct SnTask*, task)
SN_END_CLASS()

SN_END_THIN_CLASSES()

SN_BEGIN_CLASSES()

SN_BEGIN_CLASS(SnClass)
	SN_MEMBER_DATA(SnSymbol, name)
	SN_MEMBER_ROOT(struct SnObject*, instance_prototype)
SN_END_CLASS()

SN_BEGIN_CLASS(SnFunction)
	SN_MEMBER_ROOT(struct SnFunctionDescription*, desc)
	SN_MEMBER_ROOT(struct SnContext*, declaration_context)
	SN_MEMBER_DATA(uint32_t, num_variable_references)
	SN_MEMBER_ROOT_ARRAY(struct SnVariableReference*, variable_references, num_variable_references)
SN_END_CLASS()

SN_BEGIN_CLASS(SnException)
	SN_MEMBER_ROOT(struct SnString*, description)
	SN_MEMBER_ROOT(struct SnContinuation*, thrown_by)
SN_END_CLASS()

SN_END_CLASSES()


#undef SN_BEGIN_IMMEDIATE_TYPES
#undef SN_END_IMMEDIATE_TYPES
#undef SN_DECLARE_IMMEDIATE_TYPE
#undef SN_BEGIN_CLASS
#undef SN_BEGIN_THIN_CLASS
#undef SN_END_CLASS
#undef SN_BEGIN_THIN_CLASSES
#undef SN_END_THIN_CLASSES
#undef SN_BEGIN_CLASSES
#undef SN_END_CLASSES
#undef SN_MEMBER_DATA
#undef SN_MEMBER_ROOT
#undef SN_MEMBER_ROOT_ARRAY
