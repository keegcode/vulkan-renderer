#pragma once

#ifdef _MSC_VER
	#define DEBUG_BREAK() __debugbreak()
#else
	#define DEBUG_BREAK() asm("int $3")
#endif
