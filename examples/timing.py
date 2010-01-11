import lsst.meas.multifit as measMult
import lsst.afw.geom as afwGeom
import lsst.pex.policy as pexPolicy
import time

from makeImageStack import makeImageStack

def psTiming(maxIteration, depth):
    psFactory = measMult.PointSourceModelFactory()
    psModel = psFactory.makeModel(1, afwGeom.makePointD(0,0))
    exposureList = makeImageStack(psModel, depth)
    t0 = time.time()
    modelEvaluator = measMult.ModelEvaluator(psModel, exposureList)
    t1 = time.time()
    print "Construction of ModelEvaluator: %0.3f ms"%((t1-t0)*1000.0)
    print "\tUsing %d pixels in %d exposures"% \
        (modelEvaluator.getNPixels(), modelEvaluator.getNProjections())

    t0 = time.time()
    fitterPolicy = pexPolicy.Policy()
    fitterPolicy.add("terminationType", "iteration")
    fitterPolicy.add("iterationMax", maxIteration)
    fitter=measMult.SingleLinearParameterFitter(fitterPolicy)
    t1 = time.time()
    print "Construction of Fitter: %0.3f ms"%((t1-t0)*1000.0)

    t0 = time.time()
    result = fitter.apply(modelEvaluator)
    t1 = time.time()
    nIterations = result.sdqaMetrics.get("nIterations")
    print "%d iterations of Fitter: %0.3f ms"%(nIterations, (t1-t0)*1000.0)

if __name__ == "__main__":
    psTiming(5, 30)
    print ""
    psTiming(10, 30)
    print ""
    psTiming(15, 30)
    print ""
    psTiming(20, 30)
    print ""
    psTiming(5, 50)
    print ""
    psTiming(20, 50)
    print ""
    psTiming(5, 200)
    print ""
    psTiming(20, 200)




