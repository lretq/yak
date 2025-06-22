#pragma once

#include <yak/io/registry.hh>

class PciPersonality : public Personality {
	IO_OBJ_DECLARE(PciDriverPersonality);

    public:
	static constexpr long MATCH_ANY = -1;

	constexpr PciPersonality()
		: driver(nullptr)
		, vendor(MATCH_ANY)
		, device(MATCH_ANY)
		, classCode(MATCH_ANY)
		, subclass(MATCH_ANY)
	{
	}

	constexpr PciPersonality(const ClassInfo *driver, long vendor,
				 long device_id, long classCode, long subclass)
		: driver(driver)
		, vendor(vendor)
		, device(device_id)
		, classCode(classCode)
		, subclass(subclass)
	{
	}

	constexpr virtual bool isEqual(Object *other) const override
	{
		if (auto pers = other->safe_cast<PciPersonality>()) {
			if (pers->vendor != MATCH_ANY &&
			    this->vendor != MATCH_ANY &&
			    pers->vendor != this->vendor)
				return false;

			if (pers->device != MATCH_ANY &&
			    this->device != MATCH_ANY &&
			    pers->device != this->device)
				return false;

			if (pers->classCode != MATCH_ANY &&
			    this->classCode != MATCH_ANY &&
			    pers->classCode != this->classCode)
				return false;

			if (pers->subclass != MATCH_ANY &&
			    this->subclass != MATCH_ANY &&
			    pers->subclass != this->subclass)
				return false;

			return true;
		}
		return false;
	}

	constexpr virtual const ClassInfo *getDriverClass() const override
	{
		return driver;
	}

    private:
	const ClassInfo *driver;
	long vendor, device, classCode, subclass;
};
