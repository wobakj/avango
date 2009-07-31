// -*- Mode:C++ -*-

/************************************************************************\
*                                                                        *
* This file is part of Avango.                                           *
*                                                                        *
* Copyright 1997 - 2008 Fraunhofer-Gesellschaft zur Foerderung der       *
* angewandten Forschung (FhG), Munich, Germany.                          *
*                                                                        *
* Avango is free software: you can redistribute it and/or modify         *
* it under the terms of the GNU Lesser General Public License as         *
* published by the Free Software Foundation, version 3.                  *
*                                                                        *
* Avango is distributed in the hope that it will be useful,              *
* but WITHOUT ANY WARRANTY; without even the implied warranty of         *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the           *
* GNU General Public License for more details.                           *
*                                                                        *
* You should have received a copy of the GNU Lesser General Public       *
* License along with Avango. If not, see <http://www.gnu.org/licenses/>. *
*                                                                        *
* Avango is a trademark owned by FhG.                                    *
*                                                                        *
\************************************************************************/

#include "avango/FieldContainer.h"

#include <list>
#include <iterator>
#include <stdexcept>

#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

#include <avango/Assert.h>
#include <avango/ContainerPool.h>
#include <avango/WriteAction.h>
#include <avango/Field.h>
#include <avango/FieldRef.h>
#include <avango/Logger.h>
#include <avango/InputStream.h>

#include <avango/Config.h>

namespace
{
  av::Logger& logger(av::Logger::getLogger("av::FieldContainer"));

  typedef std::list<av::Link<av::FieldContainer> > FieldContainerList;

  FieldContainerList& evaluation_list = *new FieldContainerList;

  std::string empty_string;
} // namespace

AV_BASE_DEFINE_ABSTRACT(av::FieldContainer);

AV_FIELD_DEFINE(av::SFContainer);
AV_FIELD_DEFINE(av::MFContainer);

unsigned int av::FieldContainer::sEvaluateId = 0u;

av::FieldContainer::FieldContainer() :
  mEvaluateId(0u)
{
  mFlags.mNeedsEvaluation = false;
  mFlags.mFieldsCalculated = false;
  mFlags.mAlwaysEvaluate = false;
  mFlags.mIsEvaluating = false;
  mFlags.mAllowScheduling = true;

  // Register with pool and set ID to the one, generated by the pool
  // as response to registration.
  mId = av::ContainerPool::registerInstance(this);
  av::ContainerPool::notifyCreation(this);

  AV_FC_ADD_ADAPTOR_FIELD(Name,
                          boost::bind(&av::FieldContainer::getNameCB, this, _1),
                          boost::bind(&av::FieldContainer::setNameCB, this, _1));
}

/* virtual */
av::FieldContainer::~FieldContainer()
{
  av::ContainerPool::notifyDeletion(this);
  av::ContainerPool::unregisterInstance(this);

  // delete owned fields
  for (FieldInfos::iterator field = mFields.begin(); field != mFields.end(); ++field)
  {
    if (field->mField->isOwned())
      delete field->mField;
  }
}

/* static */ void
av::FieldContainer::initClass()
{
  if (!isTypeInitialized())
  {
    Distributed::initClass();

    AV_BASE_INIT_ABSTRACT(av::Distributed, av::FieldContainer, true);

    SFContainer::initClass("av::SFContainer", "av::Field");
    MFContainer::initClass("av::MFContainer", "av::Field");
  }
}

unsigned int
av::FieldContainer::addField(Field* field, const std::string& fieldName)
{
  AV_ASSERT(field);

  FieldInfo newFieldInfo;
  newFieldInfo.mField = field;
  newFieldInfo.mName = fieldName;

  mFields.push_back(newFieldInfo);
  mFieldsIndex.insert(std::make_pair(newFieldInfo.mName, mFields.size()-1));

  mFlags.mFieldsCalculated = false;

  return mFields.size() - 1;
}

av::FieldContainer::FieldInfo*
av::FieldContainer::getFieldInfo(const std::string& name)
{
  FieldsIndex::iterator iter = mFieldsIndex.find(name);
  if(iter == mFieldsIndex.end()) {
    return 0;
  } else {
    return &mFields[iter->second];
  }
}

av::FieldContainer::FieldInfo*
av::FieldContainer::getFieldInfo(unsigned int index)
{
  return (index < mFields.size() ? &mFields[index] : 0);
}

unsigned int
av::FieldContainer::getNumFields()
{
  return getFields().size();
}

/* virtual */ void
av::FieldContainer::addDynamicField(const std::string& typeName, const std::string& fieldName)
{
  Typed *typed = av::Type::createInstanceOfType(typeName);
  if (typed != 0)
  {
    Field *field = dynamic_cast<Field*>(typed);
    if (field != 0)
    {
      field->bind(this, fieldName, true);
    }
    else
    {
      delete typed;
      AVANGO_LOG(logger,logging::WARN , boost::str(boost::format("type '%1%' is no field") % typeName))
    }
  }
  else
    AVANGO_LOG(logger,logging::WARN , boost::str(boost::format("could not create instance of type '%1%'") % typeName))
}

/* virtual */ void
av::FieldContainer::addDynamicField(Field* field, const std::string& fieldName)
{
  if (field != 0)
    field->clone()->bind(this, fieldName, true);
  else
    AVANGO_LOG(logger,logging::WARN , "invalid field")
}

/* virtual */ bool
av::FieldContainer::hasField(const std::string& name)
{
  return (getFieldInfo(name) != 0);
}

av::Field*
av::FieldContainer::getField(unsigned int index)
{
  return (index < mFields.size() ? mFields[index].mField : 0);
}

av::Field*
av::FieldContainer::getField(const std::string& name)
{
  FieldInfo *field_info = getFieldInfo(name);
  return (field_info != 0 ? field_info->mField : 0);
}

const std::string&
av::FieldContainer::getFieldName(unsigned int index)
{
  return (index < mFields.size() ? mFields[index].mName : empty_string);
}

av::FieldContainer::IDType
av::FieldContainer::getId() const
{
  return mId;
}

bool
av::FieldContainer::needsEvaluation() const
{
  return mFlags.mNeedsEvaluation;
}

void
av::FieldContainer::callEvaluate()
{
  if (!mFlags.mIsEvaluating)
  {
    if (mEvaluateId != sEvaluateId)
    {
      mFlags.mIsEvaluating = true;

      AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("callEvaluate: evaluating dependencies of '%1%' @0x%2%.") % getTypeId().getName() % this));

      // Evaluate field containers that our own fields depend on
//      FieldPtrVec &fields = getFields();
//      for (FieldPtrVec::iterator field_it = fields.begin(); field_it != fields.end(); ++field_it)
//      {
//        (*field_it)->evaluateDependencies();
//      }

      // Evaluate field containers that our own fields depend on
      //TODO Changed for simplicity
      std::for_each (getFields().begin(), getFields().end(), std::mem_fun(&Field::evaluateDependencies));

      if (mFlags.mNeedsEvaluation)
      {
        AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("callEvaluate: evaluating '%1%' @0x%2% (%3$07.3lf).") % getTypeId().getName() % this % mLastChange));

        // evaluate for local sideeffect
        this->evaluateLocalSideEffect();

        // evaluate for (possibly) global sideeffect
        //  (in the distr. case only if <this> "owns" the container,
        //  i.e. is the original creator
        if (isOwned())
        {
          this->evaluate();
        }

        //reset the status of the fields
        std::for_each (getFields().begin(), getFields().end(), std::mem_fun(&Field::reset));

        mFlags.mNeedsEvaluation = mFlags.mAlwaysEvaluate;

        AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("callEvaluate: '%1%' @0x%2% evaluated.") % getTypeId().getName() % this));

      }

      mEvaluateId = sEvaluateId;
      mFlags.mIsEvaluating = false;
    }
  }
  else
  {
    AVANGO_LOG(logger,logging::WARN , boost::str(boost::format("callEvaluate: detected evaluation loop: '%1%', %2%.") % getTypeId().getName() % Name.getValue()));
  }
}

double
av::FieldContainer::lastChange() const
{
  return mLastChange;
}

void
av::FieldContainer::scheduleForEvaluation()
{
  if (!mFlags.mNeedsEvaluation)
  {
    mFlags.mNeedsEvaluation = true;

    if (!mFlags.mAllowScheduling)
      return;

    evaluation_list.push_back(Link<FieldContainer>(this));

    AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("scheduleForEvaluation: '%1%' @0x%2% scheduled, list size %3%.") % getTypeId().getName() % this % evaluation_list.size()));

  }
}

void
av::FieldContainer::touch(bool from_net)
{
  if (!from_net)
  {
#if defined(AVANGO_DISTRIBUTION_SUPPORT)
    notifyLocalChange();
#endif
  }

  scheduleForEvaluation();
}

void
av::FieldContainer::enableNotify(bool on)
{
  std::for_each (getFields().begin(), getFields().end(),
                 std::bind2nd(std::mem_fun(&Field::enableNotify), on));
}

void
av::FieldContainer::allowScheduling(bool on)
{
  mFlags.mAllowScheduling = on;
}

void
av::FieldContainer::alwaysEvaluate(bool on)
{
  if (on != mFlags.mAlwaysEvaluate)
  {
    mFlags.mAlwaysEvaluate = on;

    AVANGO_LOG(logger,logging::INFO , boost::str(boost::format("alwaysEvaluate: %1%abled 'alwaysEvaluate' for '%2%' @0x%3%") % (on ? "en" : "dis") % getTypeId().getName() % this));

    touch();
  }
}

void
av::FieldContainer::evaluateDependency(FieldContainer& dependency)
{
  if (mFlags.mIsEvaluating)
    dependency.callEvaluate();
  else
    AVANGO_LOG(logger,logging::WARN , "evaluateDependency: called from outside of evaluate");
}

/* virtual */ void
av::FieldContainer::getNameCB(const av::SFString::GetValueEvent& event)
{
  *(event.getValuePtr()) = ContainerPool::getNameByInstance(this);
}

/* virtual */ void
av::FieldContainer::setNameCB(const av::SFString::SetValueEvent& event)
{
  ContainerPool::setNameForInstance(this, event.getValue());
}

/* static */ void
av::FieldContainer::evaluateAllContainers()
{
  if (!evaluation_list.empty())
  {
    // create new evaluate traversal id
    ++sEvaluateId;

    AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("evaluateAllContainers: %1% in evaluation list") % evaluation_list.size()));

    FieldContainerList::iterator current_container = evaluation_list.begin();
    while (current_container != evaluation_list.end())
    {
      if ((*current_container).isValid())
      {
        // only evaluate if the evaluation_list has not the last reference to the object
        // TODO: evaluation_list should use weak pointers
        if ((*current_container)->referenceCount() > 1)
        {
          (*current_container)->callEvaluate();

          // setup for reschedule if requested or needed
          if ((*current_container)->mFlags.mNeedsEvaluation)
            ++current_container;
          else
            current_container = evaluation_list.erase(current_container);
        }
        else
          current_container = evaluation_list.erase(current_container);
      }
      else
      {
        current_container = evaluation_list.erase(current_container);
        AVANGO_LOG(logger,logging::WARN , "evaluateAllContainers: skipping invalid container");
      }
    }

    AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("evaluateAllContainers: %1% rescheduled") % evaluation_list.size()));

  }
}

/* static */ void
av::FieldContainer::unscheduleEvaluationForAllContainers()
{
  evaluation_list.clear();
}

unsigned int
av::FieldContainer::getNumWriteableFields()
{
  int                           count = 0;
  FieldPtrVec::const_iterator current = getFields().begin();
  FieldPtrVec::const_iterator past_of_end = getFields().end();

  while (current != past_of_end) {
    if ((*current)->isWritable())
      ++count;

    ++current;
  }

  return count;
}

void
av::FieldContainer::disconnectAllFields()
{
  FieldPtrVec& fields = getFields();
  for (FieldPtrVec::iterator eachField = fields.begin();
        eachField != fields.end();
        ++eachField)
  {
    (*eachField)->disconnect();
  }
}

std::vector<av::Field*>&
av::FieldContainer::getFields()
{
  if (!mFlags.mFieldsCalculated) {
    FieldPtrVec emptyVec(mFields.size());
    mFieldPtrs.swap(emptyVec);
    std::transform(mFields.begin(), mFields.end(), mFieldPtrs.begin(),
                   boost::bind(&FieldInfo::mField, _1));
    mFlags.mFieldsCalculated = true;
  }
  return mFieldPtrs;
}

bool
av::FieldContainer::getFieldRef(const std::string& name, av::FieldRef& fr)
{
  Field* f = getField(name);
  if (f) {
    fr.mFC = this;
    fr.mFi = f->getIndex();
    return true;
  } else {
    return false;
  }
}

void
av::FieldContainer::evaluate()
{
  AVANGO_LOG(logger,logging::TRACE , "evaluated: called");
}

void
av::FieldContainer::evaluateLocalSideEffect()
{
  AVANGO_LOG(logger,logging::TRACE , "evaluteLocalSideEffect: called");
}

void
av::FieldContainer::fieldHasChanged(const Field& /*field*/)
{
  AVANGO_LOG(logger,logging::TRACE , "fieldHasChanged: called");
}

void
av::FieldContainer::fieldHasChangedLocalSideEffect(const Field& /*field*/)
{
  AVANGO_LOG(logger,logging::TRACE , "fieldHasChangedLocalSideEffect: called");
}

void
av::FieldContainer::fieldChanged(const Field& field, bool from_net)
{
  // local side effects are always evaluated
  this->fieldHasChangedLocalSideEffect(field);

  // call the virtual function to let user code handle this
  if (isOwned())
  {
    this->fieldHasChanged(field);
  }

  AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("FieldContainer::fieldChanged: '%1%' @0x%2%: field '%3%' changed %4%.") % getTypeId().getName()  % this % field.getName() % (from_net? "from net" : "locally") ));

  // TODO timestamp for last change
  // mLastChange = pfGetFrameTimeStamp();

  // queue for evaluation
  touch(from_net);
}

void
av::FieldContainer::read(InputStream& is)
{
  FieldPtrVec& fields = getFields();

  if (is.isBinaryEnabled())
  {
    // read number of fields
    uint32_t count;

    is.read((char*) &count, sizeof(count));

    AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("read: reading %1% fields") % count));

    for (unsigned int i = 0; i < count; i++)
    {
      // read index of this field
      uint32_t index = 0;

      is.read((char*) &index, sizeof(index));

      AV_ASSERT(index < fields.size());

      // read field value
      is >> fields[index];
      fields[index]->readConnection(is);

      AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("    %1%") % fields[index]->getName()));

    }
  }
  else
  {
    // read number of fields
    int count;

    is >> count;
    for (int i=0; i<count; i++)
    {
      // read name of this field
      std::string name;
      is >> name;

      // read field value
      Field* field = getField(name);
      is >> field;
      field->readConnection(is);
    }
  }
}

#if defined(AVANGO_DISTRIBUTION_SUPPORT)
/* virtual */ void
av::FieldContainer::push(av::Msg& msg)
{

  AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("push: pushing a %1% (%2%)") % getTypeId().getName() % typeid(*this).name() ));

  FieldPtrVec& fields = getFields();
  int fields_pushed = 0;
  FieldPtrVec::iterator i;

  for (i=fields.begin(); i!=fields.end(); ++i) {
    Field* field = (*i);

    if (msg.getType() == Msg::relative) {
      // push only changed fields
      if (field->isDistributable() &&
          field->mFlags.needsDistribution) {
        av_pushMsg(msg, field);
        av_pushMsg(msg, field->getIndex());
        fields_pushed++;
        field->mFlags.needsDistribution = false;
      }
    } else {
      // push all fields
      if (field->isDistributable()) {
        av_pushMsg(msg, field);
        av_pushMsg(msg, field->getIndex());
        fields_pushed++;
      }
    }
  }

  AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("push: %1% of %2% fields.") % fields_pushed % this->getNumFields()));

  av_pushMsg(msg, fields_pushed);
}

/* virtual */ void
av::FieldContainer::pop(av::Msg& msg)
{

  AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("pop: popping a %1% (%2%).") % getTypeId().getName() % typeid(*this).name()));

  unsigned int num_fields;
  av_popMsg(msg, num_fields);

  AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("pop: %1% of %2% fields.") % num_fields % this->getNumFields()));

  AV_ASSERT(num_fields <= this->getNumFields());

  for (unsigned int i=0; i<num_fields; ++i) {
    unsigned int index;

    av_popMsg(msg, index);
    AV_ASSERT(index < this->getNumFields());
    Field* field = getField(index);

    AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("pop: (index %1%:%2%)") % index % field->getTypeId().getName()));

    av_popMsg(msg, field);
  }
}
#endif // #if defined(AVANGO_DISTRIBUTION_SUPPORT)

void
av::FieldContainer::write(OutputStream& os)
{
  FieldPtrVec& fields = getFields();

  if (os.isBinaryEnabled())
  {
    // write number of fields
    uint32_t size = getNumWriteableFields();

    AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("write: writing %1% fields") % size));

    os.write((char*) &size, sizeof(size));

    FieldPtrVec::iterator i;

    for (i=fields.begin(); i!=fields.end(); i++)
    {
      if ((*i)->isWritable())
      {

        AVANGO_LOG(logger,logging::DEBUG , boost::str(boost::format("    %1%") % (*i)->getName()));

        // write index of this field
        uint32_t index = (*i)->getIndex();
        os.write((char*) &index, sizeof(index));
        // write the field value
        (*i)->write(os);
        // write possible connections
        (*i)->writeConnection(os);
      }
    }
  }
  else
  {
    // write number of fields

#if !defined(_WIN32)
    os << getNumWriteableFields() << std::endl;
#else
    // cl of VS 8 apparently not able to resolve os << bla. Or is this resolved with Service Pack 1 ?!
    os.operator<<(getNumWriteableFields());
    os.operator<<(std::endl);
#endif

    FieldPtrVec::iterator i;

    for (i=fields.begin(); i!=fields.end(); i++)
    {
      if ((*i)->isWritable()) {
        // write name of this field
        os << (*i)->getName();
        (std::ostream&) os << ' ';
        // write the field value
        (*i)->write(os);
        os << std::endl;
        // write possible connections
        (*i)->writeConnection(os);
        os << std::endl;
      }
    }
  }
}
