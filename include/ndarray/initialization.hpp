#ifndef NDARRAY_initialization_hpp_INCLUDED
#define NDARRAY_initialization_hpp_INCLUDED

/** 
 *  \file ndarray/initialization.hpp \brief Construction functions for array.
 */

#include "ndarray/Array.hpp"

namespace ndarray {
namespace detail {

/** 
 *  \internal @class ArrayDeleter
 *  \ingroup InternalGroup
 *  \brief A boost::shared_ptr deleter functor for STL allocators.
 *
 *  This allows a shared_ptr (not a shared_array) to store memory allocated by any STL allocator.
 */
template <typename Allocator>
struct ArrayDeleter : private Allocator {

    typename Allocator::size_type _size;

    explicit ArrayDeleter(typename Allocator::size_type size, Allocator const & alloc = Allocator()) : 
        Allocator(alloc), _size(size) {}

    void operator()(typename Allocator::pointer p) { Allocator::deallocate(p,_size); }
};

/** 
 *  \internal @class AllocationInitializer
 *  \ingroup InternalGroup
 *  \brief An expression class that specifies dimensions and an allocator for a new array.
 *
 *  AllocationInitializer objects are returned by the allocate() functions, and are implicitly
 *  convertible to Array; they will typically only exist as temporaries used to construct new
 *  Arrays or shallow-assign to existing Arrays.
 *
 *  \sa allocate
 */
template <int N, typename Allocator>
class AllocationInitializer : private Allocator {
    Vector<int,N> _shape;
public:

    AllocationInitializer(Vector<int,N> const & shape, Allocator const & alloc = Allocator()) : 
        Allocator(alloc), _shape(shape) {}

    template <typename T, int C>
    operator Array<T,N,C> () const {
        typedef typename boost::remove_const<T>::type NonConst;
        typedef typename Allocator::template rebind<NonConst>::other BoundAllocator;
        typedef typename Array<T,N,C>::Core Core;
        int total = _shape.product();
        BoundAllocator alloc(*this);
        boost::shared_ptr<NonConst> owner(alloc.allocate(total),ArrayDeleter<BoundAllocator>(total,alloc));
        return Array<T,N,C>(
            owner.get(),
            Core::create(_shape,owner)
        );
    }

};

/** 
 *  \internal @class ExternalInitializer
 *  \ingroup InternalGroup
 *  \brief An expression class that allows construction of an Array from external data.
 *
 *  ExpressionInitializer objects are returned by the external() functions, and are implicitly
 *  convertible to Array; they will typically only exist as temporaries used to construct new
 *  Arrays or shallow-assign to existing Arrays.
 *
 *  \sa external
 */
template <typename T, int N>
class ExternalInitializer {
    T * _data;
    boost::shared_ptr<T> _owner;
    Vector<int,N> _shape;
    Vector<int,N> _strides;
public:
    
    ExternalInitializer(
        T * data, 
        Vector<int,N> const & shape,
        Vector<int,N> const & strides,
        boost::shared_ptr<T> const & owner
    ) : _data(data), _owner(owner), _shape(shape), _strides(strides) {}

    template <typename U, int C>
    operator Array<U,N,C> () const {
        typedef typename Array<T,N,C>::Core Core;
        return Array<T,N,C>(
            _data,
            Core::create(
                _shape,_strides,
                boost::const_pointer_cast<typename boost::remove_const<T>::type>(_owner)
            )
        );
    }
    
};

} // namespace ndarray::detail

/// \addtogroup MainGroup
/// @{

/** 
 *  \brief Create an expression that allocates uninitialized memory for an array.
 *
 *  A custom STL-style allocator can be passed as the second argument.
 *  If no allocator is passed, the default STL allocator will be used.
 *
 *  \returns A temporary object convertible to an Array with fully contiguous row-major strides.
 */
template <typename Allocator, int N>
inline detail::AllocationInitializer<N,Allocator> allocate(
    Vector<int,N> const & shape, ///< A Vector of dimensions for the new Array.
    Allocator const & alloc=Allocator() ///< An instance of an STL allocator.
) {
    return detail::AllocationInitializer<N,Allocator>(shape,alloc); 
}

/** 
 *  \brief Create an expression that allocates uninitialized memory for an array.
 *
 *  This overload uses the default STL allocator, std::allocator.
 *
 *  \returns A temporary object convertible to an Array with fully contiguous row-major strides.
 */
template <int N>
inline detail::AllocationInitializer<N,std::allocator<void> > allocate(
    Vector<int,N> const & shape ///< A Vector of dimensions for the new Array.
) {
    return detail::AllocationInitializer<N,std::allocator<void> >(shape); 
}

/** 
 *  \brief Create a new Array by copying an Expression.
 *
 *  A custom STL-style allocator can be passed as the second argument.
 *  If no allocator is passed, the default STL allocator will be used.
 */
template <typename Allocator, typename Derived>
inline Array<typename boost::remove_const<typename Derived::Element>::type,
             Derived::ND::value,Derived::ND::value>
copy(
    Expression<Derived> const & expr, ///< The Expression to copy.
    Allocator const & alloc=Allocator() ///< An instance of an STL allocator.
) {
    Array<typename boost::remove_const<typename Derived::Element>::type,
        Derived::ND::value,Derived::ND::value
        > r(
            allocate(expr.getShape(),alloc)
        );
    r = expr;
    return r;
}

/** 
 *  \brief Create a new Array by copying an Expression.
 *
 *  This overload uses the default STL allocator, std::allocator.
 */
template <typename Derived>
inline Array<typename boost::remove_const<typename Derived::Element>::type,
             Derived::ND::value,Derived::ND::value>
copy(
    Expression<Derived> const & expr ///< The Expression to copy.
) {
    Array<typename boost::remove_const<typename Derived::Element>::type,
        Derived::ND::value,Derived::ND::value> r(
        allocate(expr.getShape())
    );
    r = expr;
    return r;
}

/// \brief An enumeration for stride computation standards.
enum DataOrderEnum { ROW_MAJOR=1, COLUMN_MAJOR=2 };

/// \brief Compute row- or column-major strides for the given shape.
template <int N>
Vector<int,N> computeStrides(Vector<int,N> const & shape, DataOrderEnum order=ROW_MAJOR) {
    Vector<int,N> r(1);
    if (order == ROW_MAJOR) {
        for (int n=N-1; n > 0; --n) r[n-1] = r[n] * shape[n];
    } else {
        for (int n=0; n < N; ++n) r[n+1] = r[n] * shape[n];
    }
    return r;
}

/** 
 *  \brief Create an expression that initializes an Array with externally allocated memory.
 *
 *  No checking is done to ensure the shape, strides, and data pointers are sensible.  If no
 *  owner shared_ptr is supplied, the user must ensure the data pointer remains valid for
 *  the lifetime of the Array.
 *
 *  \returns A temporary object convertible to an Array.
 */
template <typename T, int N>
inline detail::ExternalInitializer<T,N> external(
    T * data, ///< A raw pointer to the first element of the Array.
    Vector<int,N> const & shape, ///< A Vector of dimensions for the new Array.
    Vector<int,N> const & strides,  ///< A Vector of strides for the new Array.
    boost::shared_ptr<T> const & owner = boost::shared_ptr<T>() /**< A shared_ptr that manages the
                                                                 *   lifetime of the memory. */
) {
    return detail::ExternalInitializer<T,N>(data,shape,strides,owner);
}

/** 
 *  \brief Create an expression that initializes an Array with externally allocated memory.
 *
 *  No checking is done to ensure the shape, strides, and data pointers are sensible.
 *
 *  \returns A temporary object convertible to an Array.
 */
template <typename T, int N>
inline detail::ExternalInitializer<T,N> external(
    boost::shared_ptr<T> const & owner, ///< A shared_ptr to the first element of the Array.
    Vector<int,N> const & shape, ///< A Vector of dimensions for the new Array.
    Vector<int,N> const & strides ///< A Vector of strides for the new Array.
) {
    return detail::ExternalInitializer<T,N>(owner.get(),shape,strides,owner);
}

/** 
 *  \brief Create an expression that initializes an Array with externally allocated memory.
 *
 *  No checking is done to ensure the shape, strides, and data pointers are sensible.  If no
 *  owner shared_ptr is supplied, the user must ensure the data pointer remains valid for
 *  the lifetime of the Array.
 *
 *  \returns A temporary object convertible to an Array.
 */
template <typename T, int N>
inline detail::ExternalInitializer<T,N> external(
    T * data, ///< A raw pointer to the first element of the Array.
    Vector<int,N> const & shape, ///< A Vector of dimensions for the new Array.
    DataOrderEnum order = ROW_MAJOR,  ///< Whether strides are row- or column-major.
    boost::shared_ptr<T> const & owner = boost::shared_ptr<T>() /**< A shared_ptr that manages the
                                                                 *   lifetime of the memory. */
) {
    return detail::ExternalInitializer<T,N>(data,shape,computeStrides(shape,order),owner);
}

/** 
 *  \brief Create an expression that initializes an Array with externally allocated memory.
 *
 *  No checking is done to ensure the shape, strides, and data pointers are sensible.
 *
 *  \returns A temporary object convertible to an Array.
 */
template <typename T, int N>
inline detail::ExternalInitializer<T,N> external(
    boost::shared_ptr<T> const & owner, ///< A shared_ptr to the first element of the Array.
    Vector<int,N> const & shape, ///< A Vector of dimensions for the new Array.
    DataOrderEnum order = ROW_MAJOR  ///< Whether strides are row- or column-major.
) {
    return detail::ExternalInitializer<T,N>(owner.get(),shape,computeStrides(shape,order),owner);
}

/// @}

} // namespace ndarray

#endif // !NDARRAY_initialization_hpp_INCLUDED
