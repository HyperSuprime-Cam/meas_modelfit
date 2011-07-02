import lsst.meas.multifitData 
import lsst.afw.detection
import lsst.afw.image
import lsst.meas.multifit
import lsst.meas.algorithms
import lsst.daf.persistence
import viewer
import numpy
import sys
import logging

try:
    import cPickle as pickle
except ImportError:
    import pickle

try:
    from matplotlib import pyplot
except ImportError:
    pass

fields = (("dataset", int),
          ("dataset_index", int),
          ("id", numpy.int64),
          ("psf_flux", float), 
          ("psf_flux_err", float),
          ("x", float),
          ("y", float),
          ("ixx", float),
          ("iyy", float),
          ("ixy", float),
          ("status", numpy.int64),
          ("flux", float),
          ("flux_err", float),
          ("e1", float),
          ("e2", float),
          ("r", float),
          ("e1_index", int),
          ("e2_index", int),
          ("r_index", int),
          ("src_flags", numpy.int64),
          )
algorithm = "SHAPELET_MODEL"

def fit(datasets=(0,1,2,3,4,5,6,7,8,9), **kw):
    bf = lsst.daf.persistence.ButlerFactory( \
        mapper=lsst.meas.multifitData.DatasetMapper())
    butler = bf.create()
    measurement, policy = lsst.meas.multifit.makeSourceMeasurement(**kw)
    nCoeff = measurement.getCoefficientSize()
    nRadius = measurement.getOptions().radiusStepCount
    nEllipticity = 2*measurement.getOptions().ellipticityStepCount + 1
    dtype = numpy.dtype(list(fields) + [
        ("coeff", float, nCoeff), 
        ("covar", float, (nCoeff, nCoeff)),
        ("test_points", float, (nRadius, 3)),
        ("objective_value", float, (nRadius, nEllipticity, nEllipticity)),
        ])
    tables = []
    for d in datasets:
        logging.info("Processing dataset %d" % d)
        if not butler.datasetExists("src", id=d):
            continue


        psf = butler.get("psf", id=d)
        exp = butler.get("exp", id=d)
        exp.setPsf(psf)
        sources = butler.get("src", id=d)

        table = numpy.zeros((len(sources)), dtype=dtype)
        table["dataset"] = d
        for j, src in enumerate(sources):            
            logging.debug("Fitting source %d (%d/%d)" % (src.getId(), j+1, len(sources)))
            record = table[j]
            record["id"] = src.getId()
            record["dataset_index"] = j
            record["psf_flux"] = src.getPsfFlux()
            record["psf_flux_err"] = src.getPsfFluxErr()
            record["x"] = src.getXAstrom()
            record["y"] = src.getYAstrom()
            record["ixx"] = src.getIxx()
            record["iyy"] = src.getIyy()
            record["ixy"] = src.getIxy()
            record["src_flags"] = src.getFlagForDetection()
            status = measurement.measure(exp, src)
            record["status"] = status
            record["flux"] = measurement.getFlux()
            record["flux_err"] = measurement.getFluxErr()
            ellipse = measurement.getEllipse()
            core = ellipse.getCore()
            record["e1"] = core.getE1()
            record["e2"] = core.getE2()
            record["r"] = core.getRadius()
            record["r_index"] = measurement.getRadiusIndex()
            record["e1_index"] = measurement.getE1Index()
            record["e2_index"] = measurement.getE2Index()
            record["coeff"][:] = measurement.getCoefficients()
            record["covar"][:,:] = measurement.getCovariance()
            record["test_points"][:] = measurement.getTestPoints()
            record["objective_value"][:,:,:] = measurement.getObjectiveValue()            

        tables.append(table)

    return numpy.concatenate(tables)

def build(filename, *args, **kwds):
    logging.basicConfig(level=logging.DEBUG)
    full = fit(*args, **kwds)
    outfile = open(filename, 'wb')
    pickle.dump(full, outfile, protocol=2)
    outfile.close()

def load(filename, filter_status=True, filter_flux=True, filter_flags=True, dataset=None):
    infile = open(filename, "rb")
    table = pickle.load(infile)
    if filter_status:
        table = table[table["status"] == 0]
    if filter_flux:
        table = table[table["flux"] > 0]
    if dataset is not None:
        table = table[table["dataset"] == dataset]
    if filter_flags:
        table = table[numpy.logical_not(table["src_flags"] & lsst.meas.algorithms.Flags.BAD)]
    return table

def m_radius(table):
    return (table["ixx"] + table["iyy"])**0.5

def ellipticity(table):
    return (table["e1"]**2 + table["e2"]**2)**0.5

def mag(table):
    return -2.5*numpy.log10(table["flux"])

def psf_mag(table):
    return -2.5*numpy.log10(table["psf_flux"])

def subset(table, x=None, y=None):
    """Use the current matplotlib plot limits to extract a subset from
    the given table."""
    if x is None:
        x = psf_mag(table)
    if y is None:
        y = psf_mag(table) - mag(table)
    xmin, xmax = pyplot.xlim()
    ymin, ymax = pyplot.ylim()
    return table[numpy.logical_and(
	numpy.logical_and(x >= xmin, x <= xmax),
	numpy.logical_and(y >= ymin, y <= ymax)
	)]

def plotPsfMagComparison(table, alpha=0.5):
    pyplot.figure()
    pyplot.scatter(psf_mag(table), psf_mag(table)-mag(table), alpha=alpha, linewidth=0)
    pyplot.axhline(0, color='k')
    pyplot.xlabel("psf")
    pyplot.ylabel("psf - mod")

def plotFluxFractions(table, alpha=0.3, **kw):
    measurement, policy = lsst.meas.multifit.makeSourceMeasurement(**kw)
    integration = measurement.getIntegration()
    fractions = integration[numpy.newaxis,:] * table["coeff"]
    fractions /= table["flux"][:,numpy.newaxis]
    offset = 0
    def doPlot(color):
        color[color < 0.0] = 0.0
        color[color > 1.0] = 1.0
	pyplot.scatter(psf_mag(table), psf_mag(table)-mag(table),
		       c=color, alpha=alpha, linewidth=0)
	pyplot.axhline(0, color='k')
	#pyplot.xlabel("psf")
	#pyplot.ylabel("psf - mod")
        pyplot.ylim(-2, 8)
    pyplot.figure(figsize=(10, 10))
    ax = pyplot.subplot(2, 2, 1)
    if measurement.getOptions().fitDeltaFunction:
	f = fractions[:,offset]
	doPlot(f)
	pyplot.title("psf component")
	offset += 1
    pyplot.subplot(2, 2, 3, sharex=ax, sharey=ax)
    if measurement.getOptions().fitExponential:
	f = fractions[:,offset]
	doPlot(f)
	pyplot.title("exponential component")
        offset += 1
    pyplot.subplot(2, 2, 4, sharex=ax, sharey=ax)
    if measurement.getOptions().fitDeVaucouleur:
	f = fractions[:,offset]
	doPlot(f)
	pyplot.title("de Vaucouleur component")
	offset += 1
    pyplot.subplot(2, 2, 2, sharex=ax, sharey=ax)
    if measurement.getOptions().shapeletOrder >= 0:    
	f = fractions[:,offset:].sum(axis=1)
	doPlot(f)
	pyplot.title("shapelet component")
    cax = pyplot.axes([0.93, 0.05, 0.02, 0.9])
    pyplot.colorbar(cax=cax)
    pyplot.suptitle("FRACTION OF TOTAL FLUX BY COMPONENT")
    pyplot.show()

def view(record):
    v = viewer.Viewer(record['dataset'])
    index = record["dataset_index"]
    v.plot(index)
    v.plotProfile(index)
