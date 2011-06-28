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

fields = (("dataset", int),
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
          ("src_flags", numpy.int64),
          )
algorithm = "SHAPELET_MODEL"

def fit(datasets=(0,1,2,3,4,5,6,7,8,9), **kw):
    bf = lsst.daf.persistence.ButlerFactory( \
        mapper=lsst.meas.multifitData.DatasetMapper())
    butler = bf.create()
    measurement, policy = lsst.meas.multifit.makeSourceMeasurement(**kw)
    nCoeff = measurement.getCoefficientSize()
    dtype = numpy.dtype(list(fields) + [("coeff", float, nCoeff)])
    tables = []
    for d in datasets:
        logging.info("Processing dataset %d" % d)
        if not butler.datasetExists("src", id=d):
            continue
        psf = butler.get("psf", id=d)
        exp = butler.get("exp", id=d)
        exp.setPsf(psf)
        sources = butler.get("src", id=d)

        measurePhotometry = lsst.meas.algorithms.makeMeasurePhotometry(exp)
        measurePhotometry.addAlgorithm(algorithm)
        measurePhotometry.configure(policy)

        table = numpy.zeros((len(sources)), dtype=dtype)
        table["dataset"] = d
        for j, src in enumerate(sources):
            logging.debug("Fitting source %d (%d/%d)" % (src.getId(), j+1, len(sources)))
            record = table[j]
            record["id"] = src.getId()
            record["psf_flux"] = src.getPsfFlux()
            record["psf_flux_err"] = src.getPsfFluxErr()
            record["x"] = src.getXAstrom()
            record["y"] = src.getYAstrom()
            record["ixx"] = src.getIxx()
            record["iyy"] = src.getIyy()
            record["ixy"] = src.getIxy()
            record["src_flags"] = src.getFlagForDetection()
            photom = measurePhotometry.measure(lsst.afw.detection.Peak(), src).find(algorithm)
            record["status"] = photom.getFlag()
            record["flux"] = photom.getFlux()
            record["flux_err"] = photom.getFluxErr()
            record["e1"] = photom.get("e1")
            record["e2"] = photom.get("e2")
            record["r"] = photom.get("radius")
            for i in range(nCoeff):
                record["coeff"][i] = photom.get(i, "coefficients")
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

def view(record):
    v = viewer.Viewer(record['dataset'])
    index = [s.getSourceId() for s in v.sources].index(record["id"])
    v.plot(index)
    v.plotProfile(index)
