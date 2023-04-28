#ifndef STACK_TYPE
  #error "Please define a stack type before including stack!"
#endif

#ifndef STACK_DEFAULT_RETURN
  #error "Please define a default return type for the stack!"
#endif

#define STACK_MAX_SIZE 50

struct Stack
{
  uint count;
  STACK_TYPE stack[STACK_MAX_SIZE];
};

Stack StackCreate()
{
  Stack stack;
  stack.count = 0;
  return stack;
}

void StackPush(inout Stack stack, in STACK_TYPE value)
{
  if (stack.count >= STACK_MAX_SIZE) return;

  stack.stack[stack.count] = value;
  stack.count++;
}

STACK_TYPE StackPop(inout Stack stack)
{
  if (stack.count > 0)
  {
    stack.count--;
    const STACK_TYPE value = stack.stack[stack.count];
    return value;
  }

  return STACK_DEFAULT_RETURN;
}

STACK_TYPE StackPeep(inout Stack stack)
{
  if (stack.count > 0)
  {
    return stack.stack[stack.count - 1];
  }

  return STACK_DEFAULT_RETURN;
}

bool StackIsEmpty(inout Stack stack)
{
  return stack.count <= 0;
}

void StackClear(inout Stack stack)
{
  stack.count = 0;
}
