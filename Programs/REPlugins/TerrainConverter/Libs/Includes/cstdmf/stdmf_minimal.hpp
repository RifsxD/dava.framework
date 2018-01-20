#ifndef STDMF_MINIMAL_HPP
#define STDMF_MINIMAL_HPP

#define __STDC_LIMIT_MACROS // needed for UINT16_MAX, etc.

/* scalar types comparing
 */

#include "cstdmf/cstdmf_dll.hpp"
#include <algorithm>
#include <math.h>

// User to delete pointers and zero out their memory to avoid dangling
// references.
template< class T > void bw_safe_delete( T & p )
{
	delete p;
	p = NULL;
}

// User to delete pointers to arrays and zero out their memory to avoid
// dangling references.
template< class T > void bw_safe_delete_array( T & p )
{
	delete[] p;
	p  = NULL;
}

#if defined(_WIN32)

#undef min
#define min min
#undef max
#define max max

template <class T>
inline const T & min( const T & a, const T & b )
{
	return b < a ? b : a;
}

template <class T>
inline const T & max( const T & a, const T & b )
{
	return a < b ? b : a;
}

#define MF_MIN min
#define MF_MAX max

#if !defined(NOMINMAX)//prevent warning if NOMINMAX is passed as command-line argument to compiler
#define NOMINMAX
#endif

#else

#include <stdlib.h> // Defines size_t

#define MF_MIN std::min
#define MF_MAX std::max

#endif // if defined( _WIN32 )

#define ARRAYSZ(v)					(sizeof(v) / sizeof(v[0]))
#define ARRAY_SIZE(v)				(sizeof(v) / sizeof(v[0]))

namespace BW
{
/**
 *	Static (i.e. compile-time) assert. Based off
 *	Modern C++ Design: Generic Programming and Design Patterns Applied
 *	Section 2.1
 *	by Andrei Alexandrescu
 */
template<bool> class BW_compile_time_check
{
public:
	BW_compile_time_check(...) {}
};

template<> class BW_compile_time_check<false>
{
};

#if defined( EMSCRIPTEN )

// Work around for Emscripten generating bad code from empty struct.
#define COMPILE_TIME_CHECK_ERROR_STRUCT(errormsg) struct ERROR_##errormsg { int dummy; }

#else // !defined( EMSCRIPTEN )

#define COMPILE_TIME_CHECK_ERROR_STRUCT(errormsg) struct ERROR_##errormsg {}

#endif // defined( EMSCRIPTEN )


#define BW_STATIC_ASSERT(test, errormsg)							\
	do {															\
		COMPILE_TIME_CHECK_ERROR_STRUCT( errormsg );				\
		typedef BW::BW_compile_time_check< (test) != 0 > TmplImpl;	\
		TmplImpl aTemp = TmplImpl( ERROR_##errormsg() );			\
		size_t x = sizeof( aTemp );									\
		x += 1;														\
	} while (0)

/**
 *	Template equality comparison with specialisations for float and double.
 *	Use it if you want to compare two floating point values using ==. It avoids
 *	any compiler warnings that might be generated by using == directly.
 *	@param v1 first value to compare.
 *	@param v2 second value to compare.
 */
template< typename T >
bool isEqual( const T& v1, const T& v2 )
{
	return (v1 == v2);
}


template<>
inline bool isEqual< float >( const float& f1, const float& f2 )
{
	return fabsf( f1 - f2 ) <= 0.f;
}


template<>
inline bool isEqual< double >( const double& f1, const double& f2 )
{
	return fabs( f1 - f2 ) <= 0.0;
}


/**
 *	This is shorthand for calling isEqual( @a v1, 0 ).
 */
template< typename T >
bool isZero( const T& v1 )
{
	return isEqual( v1, static_cast< T >( 0 ) );
}

} // namespace BW

#endif // STDMF_MINIMAL_HPP
