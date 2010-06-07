// -*- lsst-c++ -*-
/**
 * @file
 * Implementation of ModelEvaluator
 */
#include "lsst/meas/multifit/ModelEvaluator.h"
#include "lsst/meas/multifit/matrices.h"
#include "lsst/meas/multifit/footprintUtils.h"
#include "lsst/afw/image/MaskedImage.h"

#include <iostream>
namespace multifit = lsst::meas::multifit;

/**
 * Set the list of exposures used to evaluate the model
 *
 * This is an atomic operation which resets the state of this ModelEvaluator 
 * completely. The ModelEvaluator will not be properly initialized until after
 * this function is called.
 *
 * For each exposure in the list, a projection footprint of the model is 
 * computed. If the projection footprint has more than \c getNMinPix pixels
 * which fall within the bounding box of the exposure, then a projection is
 * generated for that exposure.
 *
 * The pixel threshold can be set on construction or by calling setNMinPix
 *
 * Data and variance vectors are constructed by concactenating all the
 * contributing pixels from each projection.
 *
 * @sa getNMinPix
 * @sa setNMinPix
 */
template<typename ImagePixel, typename MaskPixel, typename VariancePixel>
void multifit::ModelEvaluator::setExposureList(
    std::list< boost::shared_ptr< CharacterizedExposure<
            ImagePixel, MaskPixel, VariancePixel
        > > > const & exposureList
) { 
    typedef CharacterizedExposure<ImagePixel, MaskPixel, VariancePixel> CharacterizedExposure;
    typedef std::list< boost::shared_ptr<CharacterizedExposure> > ExposureList;
    typedef typename ExposureList::const_iterator ExposureIterator;
    typedef typename lsst::afw::image::Mask<MaskPixel> Mask;
    _projectionList.clear();
    _validProducts = 0;
    
    int nLinear = getLinearParameterSize();
    int nNonlinear = getNonlinearParameterSize();

    int pixSum = 0;
    
    typename CharacterizedExposure::Ptr exposure;
    ModelProjection::Ptr projection;
    FootprintConstPtr footprint;
    PsfConstPtr psf;
    WcsConstPtr wcs;    
  
    //exposures which contain fewer than _nMinPix pixels will be rejected
    //construct a list containing only those exposure which were not rejected
    ExposureList goodExposureList;
    MaskPixel bitmask = Mask::getPlaneBitMask("BAD") | 
        Mask::getPlaneBitMask("INTRP") | Mask::getPlaneBitMask("SAT") | 
        Mask::getPlaneBitMask("CR") | Mask::getPlaneBitMask("EDGE");

    // loop to create projections
    for(ExposureIterator i(exposureList.begin()), end(exposureList.end());
        i != end; ++i
    ) {
        exposure = *i;
        psf = exposure->getPSF();
        wcs = exposure->getWcs();        
        footprint = _model->computeProjectionFootprint(psf, wcs);

        footprint = clipAndMaskFootprint<MaskPixel>(
            *footprint, exposure->getMaskedImage().getMask(),
            bitmask
        );
        //ignore exposures with too few contributing pixels        
        if (footprint->getNpix() > _nMinPix) {
            _projectionList.push_back(
                _model->makeProjection(psf, wcs, footprint)
            );
            goodExposureList.push_back(exposure);

            pixSum += footprint->getNpix();
        }
    }

    //  allocate matrix buffers
    ndarray::shallow(_dataVector) = ndarray::allocate<Allocator>(ndarray::makeVector(pixSum));
    ndarray::shallow(_varianceVector) = ndarray::allocate<Allocator>(ndarray::makeVector(pixSum));

    ndarray::shallow(_modelImage) = ndarray::allocate<Allocator>(ndarray::makeVector(pixSum));
    ndarray::shallow(_linearParameterDerivative) = ndarray::allocate<Allocator>(
        ndarray::makeVector(nLinear, pixSum)
    );
    ndarray::shallow(_nonlinearParameterDerivative) = ndarray::allocate<Allocator>(
        ndarray::makeVector(nNonlinear, pixSum)
    );    
    
    int nPix;
    int pixelStart = 0, pixelEnd;

    ExposureIterator exposureIter(goodExposureList.begin());

    //loop to assign matrix buffers to each projection Frame
    for(ProjectionIterator i(_projectionList.begin()), end(_projectionList.end()); 
        i != end; ++i
    ) {
        ModelProjection & projection(**i);
        nPix = projection.getFootprint()->getNpix();
        pixelEnd = pixelStart + nPix;

        // compress the exposure using the footprint
        compressImage(
            *projection.getFootprint(), 
            (*exposureIter)->getMaskedImage(), 
            _dataVector[ndarray::view(pixelStart, pixelEnd)], 
            _varianceVector[ndarray::view(pixelStart, pixelEnd)] 
        );

        //set modelImage buffer
        projection.setModelImageBuffer(
            _modelImage[ndarray::view(pixelStart, pixelEnd)]
        );
        
        //set linear buffer
        projection.setLinearParameterDerivativeBuffer(
            _linearParameterDerivative[ndarray::view()(pixelStart, pixelEnd)]
        );
        //set nonlinear buffer
        projection.setNonlinearParameterDerivativeBuffer(
            _nonlinearParameterDerivative[ndarray::view()(pixelStart, pixelEnd)]
        );

        pixelStart = pixelEnd;
        ++exposureIter;
    }   
}

/**
 * Compute the value of the model at every contributing pixel of every exposure
 *
 * @sa ModelProjection::computeModelImage
 */
ndarray::Array<multifit::Pixel const, 1, 1> multifit::ModelEvaluator::computeModelImage() {
    if(!(_validProducts & MODEL_IMAGE)) {
        for( ProjectionIterator i(_projectionList.begin()), end(_projectionList.end());
             i  != end; ++i
        ) {
            (*i)->computeModelImage();
        }
    }    
    return _modelImage;
}

/**
 * Compute the derivative of the model with respect to its linear parameters
 *
 * @sa ModelProjection::computeLinearParameterDerivative
 */
ndarray::Array<multifit::Pixel const, 2, 2> 
multifit::ModelEvaluator::computeLinearParameterDerivative() {
    if (!(_validProducts & LINEAR_PARAMETER_DERIVATIVE)) {
        for( ProjectionIterator i(_projectionList.begin()), 
             end(_projectionList.end()); i  != end; ++i
        ) {
            (*i)->computeLinearParameterDerivative();
        }
        _validProducts |= LINEAR_PARAMETER_DERIVATIVE;
    }    
    return _linearParameterDerivative;
}

/**
 * Compute the derivative of the model with respect to its nonlinear parameters
 *
 * @sa ModelProjection::computeNonlinearParameterDerivative
 */
ndarray::Array<multifit::Pixel const, 2, 2> multifit::ModelEvaluator::computeNonlinearParameterDerivative() {
    if(!(_validProducts & NONLINEAR_PARAMETER_DERIVATIVE)) {
        for( ProjectionIterator i(_projectionList.begin()), 
             end(_projectionList.end()); i  != end; ++i
        ) {
            (*i)->computeNonlinearParameterDerivative();
        }
        _validProducts |= NONLINEAR_PARAMETER_DERIVATIVE;
    }    
    return _nonlinearParameterDerivative;
}



//explicit templating
template void multifit::ModelEvaluator::setExposureList<float, 
    lsst::afw::image::MaskPixel, lsst::afw::image::VariancePixel>
(
    std::list<boost::shared_ptr<CharacterizedExposure<float> > > const &
);
template void multifit::ModelEvaluator::setExposureList<double, 
    lsst::afw::image::MaskPixel, lsst::afw::image::VariancePixel>
(
    std::list<boost::shared_ptr<CharacterizedExposure<double> > > const &
);
