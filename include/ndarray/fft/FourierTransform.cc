#include "ndarray/fft/FFTWTraits.hpp"
#include "ndarray/fft/FourierTransform.hpp"

namespace ndarray {

template <typename T, int N> 
template <int M>
Array<T,M,M>
FourierTransform<T,N>::initializeX(Vector<int,M> const & shape) {
    OwnerX xOwner = detail::FFTWTraits<T>::allocateX(shape.product());
    return Array<T,M,M>(external(xOwner,shape,ROW_MAJOR));
}

template <typename T, int N> 
template <int M>
FourierArray<T,M,M>
FourierTransform<T,N>::initializeK(Vector<int,M> const & shape) {
    Vector<int,M> kShape(shape);
    kShape[M-1] = detail::FourierTraits<T>::computeLastDimensionSize(shape[M-1]);
    OwnerK kOwner = detail::FFTWTraits<T>::allocateK(kShape.product());
    return FourierArray<T,M,M>(shape[M-1],external(kOwner,kShape,ROW_MAJOR));
}

template <typename T, int N> 
template <int M>
void
FourierTransform<T,N>::initialize(
    Vector<int,M> const & shape, 
    Array<T,M,M> & x,
    FourierArray<T,M,M> & k
) {
    if (x.empty()) shallow(x) = initializeX(shape);
    if (k.empty()) shallow(k) = initializeK(shape);
    NDARRAY_ASSERT(x.getShape() == shape);
    NDARRAY_ASSERT(std::equal(shape.begin(),shape.end()-1,k.getShape().begin()));
    NDARRAY_ASSERT(k.getRealSize() == shape[M-1]);
}

template <typename T, int N> 
typename FourierTransform<T,N>::Ptr
FourierTransform<T,N>::planForward(
    Index const & shape, 
    typename FourierTransform<T,N>::ArrayX & x,
    typename FourierTransform<T,N>::ArrayK & k
) {
    initialize(shape,x,k);
    return Ptr(
        new FourierTransform(
            detail::FFTWTraits<T>::forward(
                N, shape.begin(), 1,
                x.getData(), NULL, 1, 0,
                k.getData(), NULL, 1, 0,
                FFTW_MEASURE | FFTW_DESTROY_INPUT
            ),
            x.getOwner(),
            k.getOwner()
        )
    );
}

template <typename T, int N>
typename FourierTransform<T,N>::Ptr
FourierTransform<T,N>::planInverse(
    Index const & shape,
    typename FourierTransform<T,N>::ArrayK & k,
    typename FourierTransform<T,N>::ArrayX & x
) {
    initialize(shape,x,k);
    return Ptr(
        new FourierTransform(
            detail::FFTWTraits<T>::inverse(
                N, shape.begin(), 1,
                k.getData(), NULL, 1, 0,
                x.getData(), NULL, 1, 0,
                FFTW_MEASURE | FFTW_DESTROY_INPUT
            ),
            x.getOwner(),
            k.getOwner()
        )
    );
}

template <typename T, int N>
typename FourierTransform<T,N>::Ptr
FourierTransform<T,N>::planMultiplexForward(
    MultiplexIndex const & shape,
    typename FourierTransform<T,N>::MultiplexArrayX & x,
    typename FourierTransform<T,N>::MultiplexArrayK & k
) {
    initialize(shape,x,k);
    return Ptr(
        new FourierTransform(
            detail::FFTWTraits<T>::forward(
                N, shape.begin()+1, shape[0],
                x.getData(), NULL, 1, x.template getStride<0>(),
                k.getData(), NULL, 1, k.template getStride<0>(),
                FFTW_MEASURE | FFTW_DESTROY_INPUT
            ),
            x.getOwner(),
            k.getOwner()
        )
    );
}

template <typename T, int N> 
typename FourierTransform<T,N>::Ptr
FourierTransform<T,N>::planMultiplexInverse(
    MultiplexIndex const & shape,
    typename FourierTransform<T,N>::MultiplexArrayK & k,
    typename FourierTransform<T,N>::MultiplexArrayX & x
) {
    initialize(shape,x,k);
    return Ptr(
        new FourierTransform(
            detail::FFTWTraits<T>::inverse(
                N, shape.begin()+1, shape[0],
                k.getData(), NULL, 1, k.template getStride<0>(),
                x.getData(), NULL, 1, x.template getStride<0>(),
                FFTW_MEASURE | FFTW_DESTROY_INPUT
            ),
            x.getOwner(),
            k.getOwner()
        )
    );
}

template <typename T, int N>
void FourierTransform<T,N>::execute() {
    detail::FFTWTraits<T>::execute(
        reinterpret_cast<typename detail::FFTWTraits<T>::Plan>(_plan)
    );
}

template <typename T, int N>
FourierTransform<T,N>::~FourierTransform() {
    detail::FFTWTraits<T>::destroy(
        reinterpret_cast<typename detail::FFTWTraits<T>::Plan>(_plan)
    );
}

} // namespace ndarray
