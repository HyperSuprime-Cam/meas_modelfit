// -*- lsst-c++ -*-
/*
 * LSST Data Management System
 * Copyright 2008-2013 LSST Corporation.
 *
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the LSST License Statement and
 * the GNU General Public License along with this program.  If not,
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */

#include "Eigen/LU"

#include "ndarray/eigen.h"

#include "lsst/pex/exceptions.h"
#include "lsst/afw/table/io/OutputArchive.h"
#include "lsst/afw/table/io/InputArchive.h"
#include "lsst/afw/table/io/CatalogVector.h"
#include "lsst/meas/multifit/priors.h"
#include "lsst/meas/multifit/integrals.h"

namespace tbl = lsst::afw::table;

namespace lsst { namespace meas { namespace multifit {

//------------- MixturePrior --------------------------------------------------------------------------------

MixturePrior::MixturePrior(PTR(Mixture const) mixture) : _mixture(mixture) {}

Scalar MixturePrior::marginalize(
    Vector const & gradient, Matrix const & fisher,
    ndarray::Array<Scalar const,1,1> const & parameters
) const {
    return integrateGaussian(gradient, fisher) - std::log(_mixture->evaluate(parameters.asEigen()));
}

Scalar MixturePrior::evaluate(
    ndarray::Array<Scalar const,1,1> const & parameters,
    ndarray::Array<Scalar const,1,1> const & amplitudes
) const {
    if ((amplitudes.asEigen<Eigen::ArrayXpr>() < 0.0).any()) {
        return 0.0;
    } else {
        return _mixture->evaluate(parameters.asEigen());
    }
}

namespace {

class EllipseUpdateRestriction : public Mixture::UpdateRestriction {
public:

    virtual void restrictMu(Vector & mu) const {
        mu[0] = 0.0;
        mu[1] = 0.0;
    }

    virtual void restrictSigma(Matrix & sigma) const {
        sigma(0,0) = sigma(1,1) = 0.5*(sigma(0,0) + sigma(1,1));
        sigma(0,1) = sigma(0,1) = 0.0;
        sigma(0,2) = sigma(2,0) = sigma(1,2) = sigma(2,1) = 0.5*(sigma(0,2) + sigma(1,2));
    }

    EllipseUpdateRestriction() : Mixture::UpdateRestriction(3) {}

};

} // anonymous

Mixture::UpdateRestriction const & MixturePrior::getUpdateRestriction() {
    static EllipseUpdateRestriction const instance;
    return instance;
}

namespace {

class MixturePriorPersistenceKeys : private boost::noncopyable {
public:
    tbl::Schema schema;
    tbl::Key<int> mixture;

    static MixturePriorPersistenceKeys const & get() {
        static MixturePriorPersistenceKeys const instance;
        return instance;
    }
private:
    MixturePriorPersistenceKeys() :
        schema(),
        mixture(schema.addField<int>("mixture", "archive ID of mixture"))
    {
        schema.getCitizen().markPersistent();
    }
};

class MixturePriorFactory : public tbl::io::PersistableFactory {
public:

    virtual PTR(tbl::io::Persistable)
    read(InputArchive const & archive, CatalogVector const & catalogs) const {
        MixturePriorPersistenceKeys const & keys = MixturePriorPersistenceKeys::get();
        LSST_ARCHIVE_ASSERT(catalogs.size() == 1u);
        LSST_ARCHIVE_ASSERT(catalogs.front().size() == 1u);
        LSST_ARCHIVE_ASSERT(catalogs.front().getSchema() == keys.schema);
        tbl::BaseRecord const & record = catalogs.front().front();
        return boost::make_shared<MixturePrior>(archive.get< Mixture >(record.get(keys.mixture)));
    }

    explicit MixturePriorFactory(std::string const & name) : tbl::io::PersistableFactory(name) {}

};

std::string getMixturePriorPersistenceName() { return "MixturePrior"; }

MixturePriorFactory mixturePriorRegistration(getMixturePriorPersistenceName());

} // anonymous

std::string MixturePrior::getPersistenceName() const { return getMixturePriorPersistenceName(); }

void MixturePrior::write(OutputArchiveHandle & handle) const {
    MixturePriorPersistenceKeys const & keys = MixturePriorPersistenceKeys::get();
    tbl::BaseCatalog catalog = handle.makeCatalog(keys.schema);
    PTR(tbl::BaseRecord) record = catalog.addNew();
    record->set(keys.mixture, handle.put(_mixture));
    handle.saveCatalog(catalog);
}

}}} // namespace lsst::meas::multifit
