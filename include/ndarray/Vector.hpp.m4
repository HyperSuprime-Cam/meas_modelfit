include(`Vector.macros.m4')dnl
changecom(`###')dnl
#ifndef NDARRAY_Vector_hpp_INCLUDED
#define NDARRAY_Vector_hpp_INCLUDED

/// @file ndarray/Vector.hpp Definition for Vector.

#include <boost/iterator/reverse_iterator.hpp>
#include <boost/mpl/int.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/repetition/repeat_from_to.hpp>
#include <boost/preprocessor/repetition/enum.hpp>

#include <iostream>

#include "ndarray/types.hpp"

/// \cond MACROS
#define NDARRAY_MAKE_VECTOR_MAX 8

#define NDARRAY_MAKE_VECTOR_ARG_SPEC(Z,I,DATA) T v ## I
#define NDARRAY_MAKE_VECTOR_SET_SPEC(Z,I,DATA) r[I] = v ## I;

#define NDARRAY_MAKE_VECTOR_SPEC(Z,N,DATA)                      \
    template <typename T>                                       \
    inline Vector<T,N> makeVector(                              \
        BOOST_PP_ENUM(N,NDARRAY_MAKE_VECTOR_ARG_SPEC,unused)    \
    ) {                                                         \
        Vector<T,N> r;                                          \
        BOOST_PP_REPEAT(N,NDARRAY_MAKE_VECTOR_SET_SPEC,unused)  \
        return r;                                               \
    }

/// \endcond

namespace ndarray {

/// \addtogroup VectorGroup
/// @{

/** 
 *  @class Vector
 *  \brief A fixed-size 1D array class.
 *
 *  Vector (with T==int) is primarily used as the data
 *  type for the shape and strides attributes of Array.
 *  
 *  Vector is implemented almost exactly as a non-aggregate
 *  boost::array, but with the addition of mathematical
 *  operators and a few other utility functions.
 */
template <
    typename T, ///< Data type.
    int N       ///< Number of elements.
    >
struct Vector {
    typedef T Element;
    typedef T Value;
    typedef T & Reference;
    typedef T const & ConstReference;
    typedef T * Iterator;
    typedef T const * ConstIterator;
    typedef boost::mpl::int_<N> ND;
    
    typedef Value value_type;
    typedef Iterator iterator;
    typedef ConstIterator const_iterator;
    typedef Reference reference;
    typedef ConstReference const_reference;
    typedef boost::reverse_iterator<T*> reverse_iterator;
    typedef boost::reverse_iterator<const T*> const_reverse_iterator;
    typedef T * pointer;
    typedef int difference_type;
    typedef int size_type;

    size_type size() const { return N; }           ///< \brief Return the size of the Vector.
    size_type max_size() const { return N; }       ///< \brief Return the size of the Vector.
    bool empty() const { return N==0; }            ///< \brief Return true if size() == 0.
    /// \brief Return an iterator to the beginning of the Vector.
    iterator begin() { return elems; }
    /// \brief Return a const_iterator to the beginning of the Vector.
    const_iterator begin() const { return elems; }
    /// \brief Return an iterator to the end of the Vector.
    iterator end() { return elems+N; }
    /// \brief Return a const_iterator to the end of the Vector.
    const_iterator end() const { return elems+N; }
    /// \brief Return a reverse_iterator to the beginning of the reversed Vector.
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    /// \brief Return a const_reverse_iterator to the beginning of the reversed Vector.
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    /// \brief Return a reverse_iterator to the end of the reversed Vector.
    reverse_iterator rend() { return reverse_iterator(begin()); }
    /// \brief Return a const_reverse_iterator to the end of the reversed Vector.
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

    /// \brief Return a reference to the first element.
    reference front() { return *elems; }
    /// \brief Return a reference to the last element.
    reference back() { return *(elems+N-1); }
    /// \brief Return a const_reference to the first element.
    const_reference front() const { return *elems; }
    /// \brief Return a const_reference to the last element.
    const_reference back() const { return *(elems+N-1); }

    /// \brief Return a reference to the element with the given index.
    reference operator[](int i) { return elems[i]; }
    /// \brief Return a const_reference to the element with the given index.
    const_reference operator[](int i) const { return elems[i]; }

    /// \brief Create a new Vector that is a subset of this.
    template <int Start, int Stop>
    Vector<T,Stop-Start> getRange() const {
        Vector<T,Stop-Start> r;
        std::copy(elems + Start, elems+Stop, r.elems);
        return r;
    }

    /// \brief Create a new Vector from the first M elements of this.
    template <int M> Vector<T,M> first() const {
        Vector<T,M> r;
        std::copy(elems,elems+M,r.elems);
        return r;
    }

    /// \brief Create a new Vector from the last M elements of this.
    template <int M> Vector<T,M> last() const {
        Vector<T,M> r;
        std::copy(elems+(N-M),elems+N,r.elems);
        return r;
    }

    /** \brief Stream output. */
    friend std::ostream& operator<<(std::ostream& os, Vector<T,N> const & obj) {
        os << "(";
        std::copy(obj.begin(),obj.end(),std::ostream_iterator<T>(os,","));
        return os << ")";
    }

    /**
     *  \brief Default constructor.
     *
     *  Initializes the elements to zero.
     */
    Vector() { this->template operator=(0); }

    /// \brief Construct with copies of a scalar.
    template <typename U>
    explicit Vector(U scalar) {
        this->template operator=(scalar);
    }

    /// \brief Converting copy constructor.
    template <typename U>
    explicit Vector(Vector<U,N> const & other) {
        this->template operator=(other);
    }

    /// \brief Return true if elements of other are equal to the elements of this.
    bool operator==(Vector const & other) const {
        return std::equal(begin(),end(),other.begin());
    }

    /// \brief Return false if any elements of other are not equal to the elements of this.
    bool operator!=(Vector const & other) const {
        return !(*this == other);
    }

    /// \brief Return the sum of all elements.
    T sum() const {
        T r = 0;
        ConstIterator const i_end = end();
        for (ConstIterator i=begin; i != i_end; ++i) r += (*i);
        return r;
    }

    /// \brief Return the product of all elements.
    T product() const {
        T r = 1;
        ConstIterator const i_end = this->end();
        for (ConstIterator i = this->begin(); i != i_end; ++i) r *= (*i);
        return r;
    }

    VECTOR_ASSIGN(=)
    VECTOR_ASSIGN(+=)
    VECTOR_ASSIGN(-=)
    VECTOR_ASSIGN(*=)
    VECTOR_ASSIGN(/=)
    VECTOR_ASSIGN(%=)
    VECTOR_ASSIGN(&=)
    VECTOR_ASSIGN(^=)
    VECTOR_ASSIGN(|=)
    VECTOR_ASSIGN(<<=)
    VECTOR_ASSIGN(>>=)

    T elems[N];
};

/// \brief Concatenate two Vectors into a single long Vector.
template <typename T, int N, int M>
inline Vector<T,N+M> concatenate(Vector<T,N> const & a, Vector<T,M> const & b) {
    Vector<T,N+M> r;
    std::copy(a.begin(),a.end(),r.begin());
    std::copy(b.begin(),b.end(),r.begin()+N);
    return r;
}

/// \brief Return a new Vector with the given scalar appended to the original.
template <typename T, int N>
inline Vector<T,N+1> concatenate(Vector<T,N> const & a, T const & b) {
    Vector<T,N+1> r;
    std::copy(a.begin(),a.end(),r.begin());
    r[N] = b;
    return r;
}

/// \brief Return a new Vector with the given scalar prepended to the original.
template <typename T, int N>
inline Vector<T,N+1> concatenate(T const & a, Vector<T,N> const & b) {
    Vector<T,N+1> r;
    r[0] = a;
    std::copy(b.begin(),b.end(),r.begin()+1);
    return r;
}

#ifndef DOXYGEN
BOOST_PP_REPEAT_FROM_TO(1, NDARRAY_MAKE_VECTOR_MAX, NDARRAY_MAKE_VECTOR_SPEC, unused)
#else
/**
 *  \brief Variadic constructor for Vector. 
 *
 *  Defined for N in [0 - NDARRAY_MAKE_VECTOR_MAX).
 */
template <typename T, int N>
Vector<T,N> makeVector(T v1, T v2, ..., T vN);
#endif

/** \brief Unary bitwise NOT for Vector. */
template <typename T, int N>
inline Vector<T,N> operator~(Vector<T,N> const & vector) {
    Vector<T,N> r(vector);
    for (typename Vector<T,N>::Iterator i = r.begin(); i != r.end(); ++i) (*i) = ~(*i);
    return r;    
}

/** \brief Unary negation for Vector. */
template <typename T, int N>
inline Vector<T,N> operator!(Vector<T,N> const & vector) {
    Vector<T,N> r(vector);
    for (typename Vector<T,N>::Iterator i = r.begin(); i != r.end(); ++i) (*i) = !(*i);
    return r;
}

VECTOR_BINARY_OP(+)
VECTOR_BINARY_OP(-)
VECTOR_BINARY_OP(*)
VECTOR_BINARY_OP(/)
VECTOR_BINARY_OP(%)
VECTOR_BINARY_OP(&)
VECTOR_BINARY_OP(^)
VECTOR_BINARY_OP(|)
VECTOR_BINARY_OP(<<)
VECTOR_BINARY_OP(>>)

/// @}

}

#endif // !NDARRAY_Vector_hpp_INCLUDED
