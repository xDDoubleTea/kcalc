#define DEFINE_STACK(NAME, TYPE)                                        \
	typedef struct {                                                \
		TYPE *data;                                             \
		int top;                                                \
		int cap;                                                \
	} NAME##Stack;                                                  \
	static inline void NAME##_stack_init(NAME##Stack *s, TYPE *arr, \
					     int cap)                   \
	{                                                               \
		s->data = arr;                                          \
		s->top = -1;                                            \
		s->cap = cap;                                           \
	}                                                               \
	static inline int NAME##_stack_push(NAME##Stack *s, TYPE val)   \
	{                                                               \
		if (s->top + 1 >= s->cap)                               \
			return -1;                                      \
		s->top++;                                               \
		s->data[s->top] = val;                                  \
		return 0;                                               \
	}                                                               \
	static inline int NAME##_stack_pop(NAME##Stack *s, TYPE *out)   \
	{                                                               \
		if (s->top < 0)                                         \
			return -1;                                      \
		*out = s->data[s->top--];                               \
		return 0;                                               \
	}                                                               \
	static inline TYPE *NAME##_stack_top(NAME##Stack *s)            \
	{                                                               \
		return (s->top < 0) ? NULL : s->data + s->top;          \
	}                                                               \
	static inline int NAME##_stack_size(NAME##Stack *s)             \
	{                                                               \
		return s->top + 1;                                      \
	}                                                               \
	static inline void NAME##_stack_free(NAME##Stack *s)            \
	{                                                               \
		kfree(s->data);                                         \
	}                                                               \
	static inline int NAME##_stack_empty(NAME##Stack *s)            \
	{                                                               \
		return s->top < 0;                                      \
	}
