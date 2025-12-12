#include "PersonalVehicles.hpp"
#include "core/backend/FiberPool.hpp"
#include "core/backend/ScriptMgr.hpp"
#include "game/backend/Self.hpp"
#include "game/gta/data/VehicleValues.hpp"
#include "game/gta/Natives.hpp"
#include "game/gta/ScriptFunction.hpp"
#include "game/gta/ScriptLocal.hpp"
#include "game/gta/ScriptGlobal.hpp"
#include "game/gta/Stats.hpp"
#include "types/script/globals/GPBD_FM.hpp"
#include "types/script/globals/MPSV.hpp"
#include "types/script/globals/FreemodeGeneral.hpp"
#include "types/script/globals/g_SavedMPGlobals.hpp"

#define MAX_GARAGE_NUM 33

namespace YimMenu
{
	// TO-DO: Use script functions for these instead?

	static int GetPropertyGarageOffset(int property)
	{
		//... (same as before, no changes needed here)
	}

	static int GetPropertyGarageSize(int property)
	{
		//... (same as before, no changes needed here)
	}

	static int GetPropertyStatState(int property)
	{
		//... (same as before, no changes needed here)
	}

	static std::string GetStaticPropertyName(int property, int garageSlotIterator)
	{
		//... (same as before, no changes needed here)
	}

	PersonalVehicles::PersonalVehicle::PersonalVehicle(int id, MPSV_Entry* data) :
	m_Id(id),
	m_Data(data)
	{
		m_Model = m_Data->VehicleModel;
		m_Plate = m_Data->NumberPlateText;
		m_Name  = std::format("{} ({})", HUD::GET_FILENAME_FOR_AUDIO_CONVERSATION(VEHICLE::GET_DISPLAY_NAME_FROM_VEHICLE_MODEL(m_Model)), m_Plate);

		SetGarage();
	}

	int PersonalVehicles::PersonalVehicle::GetId()
	{
		return m_Id;
	}

	MPSV_Entry* PersonalVehicles::PersonalVehicle::GetData()
	{
		return m_Data;
	}

	joaat_t PersonalVehicles::PersonalVehicle::GetModel()
	{
		return m_Model;
	}

	std::string PersonalVehicles::PersonalVehicle::GetPlate()
	{
		return m_Plate;
	}

	std::string PersonalVehicles::PersonalVehicle::GetName()
	{
		return m_Name;
	}

	std::string PersonalVehicles::PersonalVehicle::GetGarage()
	{
		return m_Garage;
	}

	void PersonalVehicles::PersonalVehicle::SetGarage()
	{
		for (int propertyIterator = 0; propertyIterator < MAX_GARAGE_NUM + 4; propertyIterator++)
		{
			auto propertyStatState = GetPropertyStatState(propertyIterator);
			if (propertyStatState > 0)
			{
				auto garageSize   = GetPropertyGarageSize(propertyIterator);
				auto garageOffset = GetPropertyGarageOffset(propertyIterator);
				for (int garageSlotIterator = 1; garageSlotIterator <= garageSize; garageSlotIterator++)
				{
					auto itemInSlot = *ScriptGlobal(1940530).At(garageOffset).At(garageSlotIterator).As<int*>() - 1;
					if (itemInSlot == m_Id)
					{
						auto staticPropertyString = GetStaticPropertyName(propertyIterator, garageSlotIterator);
						if (staticPropertyString.empty())
						{
							m_Garage = HUD::GET_FILENAME_FOR_AUDIO_CONVERSATION(ScriptGlobal(1312335).At(propertyStatState, 1951).At(16).As<const char*>());
						}
						else
						{
							m_Garage = staticPropertyString;
						}
						return;
					}
				}
			}
		}
	}

	bool PersonalVehicles::PersonalVehicle::Despawn()
	{
		if (auto veh = GetCurrentHandle(); veh.IsValid())
		{
			veh.BringToHalt();
			m_Data->PersonalVehicleFlags.Clear(ePersonalVehicleFlags::TRIGGER_SPAWN_TOGGLE);

			// Wait for the vehicle to despawn
			for (int i = 0; veh.IsValid(); i++)
			{
				ScriptMgr::Yield(100ms);
				if (i > 30)
				{
					LOG(WARNING) << "Despawn() Timed out despawning Personal Vehicle.";
					return false;
				}
			}
		}

		return true;
	}

	bool PersonalVehicles::PersonalVehicle::Repair()
	{
		if (m_Data->PersonalVehicleFlags.IsSet(ePersonalVehicleFlags::DESTROYED) && m_Data->PersonalVehicleFlags.IsSet(ePersonalVehicleFlags::HAS_INSURANCE))
		{
			m_Data->PersonalVehicleFlags.Clear(ePersonalVehicleFlags::DESTROYED);
			m_Data->PersonalVehicleFlags.Clear(ePersonalVehicleFlags::IMPOUNDED);
			m_Data->PersonalVehicleFlags.Clear(ePersonalVehicleFlags::UNK2);
			return true;
		}

		return false;
	}

	bool PersonalVehicles::PersonalVehicle::Request(bool bring)
	{
		if (auto freemodeGeneral = FreemodeGeneral::Get())
		{
			if (freemodeGeneral->RequestedPersonalVehicleId != -1)
				return false;

			if (!GetCurrent()->Despawn())
				return false;

			Repair();
			ScriptMgr::Yield(100ms);

			if (bring)
				freemodeGeneral->NodeDistanceCheck = 0;
			freemodeGeneral->PersonalVehicleRequested   = TRUE;
			freemodeGeneral->Exec1Impound               = FALSE;
			freemodeGeneral->RequestedPersonalVehicleId = m_Id;

			ScriptMgr::Yield(100ms);
			*ScriptLocal("freemode"_J, 19447).At(176).As<int*>() = 0;

			if (bring)
			{
				for (int i = 0; !GetCurrentHandle().IsValid(); i++)
				{
					ScriptMgr::Yield(100ms);
					if (i > 30)
						break;
				}

				if (auto veh = GetCurrentHandle(); veh.IsValid())
				{
					auto coords = Self::GetPed().GetPosition();
					auto heading = Self::GetPed().GetHeading();
					veh.SetPosition(coords);
					veh.SetHeading(heading);
					veh.SetOnGroundProperly();
					Self::GetPed().SetInVehicle(veh);
				}
			}

			return true;
		}

		return false;
	}

	Vehicle PersonalVehicles::PersonalVehicle::Clone(rage::fvector3 coords, float heading)
	{
		if (auto veh = Vehicle::Create(m_Model, coords, heading))
		{
			auto oldVal = m_Data->IsPersonalVehicle;

			m_Data->IsPersonalVehicle = 0;

			static ScriptFunction applyMPSVData("freemode"_J, ScriptPointer("ApplyMPSVData", "5D ? ? ? 38 2A 71").Add(1).Rip());
			applyMPSVData.Call<void>(veh.GetHandle(), m_Data, true, true, false);

			m_Data->IsPersonalVehicle = oldVal;
			return veh;
		}

		return nullptr;
	}

	std::unique_ptr<PersonalVehicles::PersonalVehicle> PersonalVehicles::GetCurrentImpl()
	{
		auto savedMPGlobals = g_SavedMPGlobals::Get();
		auto MPSV           = MPSV::Get();
		if (savedMPGlobals && MPSV)
		{
			auto id = savedMPGlobals->Entries[0].GeneralSaved.LastSavedCar;
			auto data = &MPSV->Entries[id];
			return std::make_unique<PersonalVehicle>(id, data);
		}

		return nullptr;
	}

	Vehicle PersonalVehicles::GetCurrentHandleImpl()
	{
		if (auto freemodeGeneral = FreemodeGeneral::Get())
		{
			if (auto veh = freemodeGeneral->PersonalVehicleIndex; veh != -1)
				return Vehicle(veh);
		}

		return nullptr;
	}

	void PersonalVehicles::UpdateImpl()
	{
		const auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::seconds>(now - m_LastUpdate) < 10s)
			return;

		m_LastUpdate = std::chrono::steady_clock::now();

		FiberPool::Push([] {
			RegisterVehicles();
			RegisterGarages();
		});
	}

	void PersonalVehicles::RegisterVehiclesImpl()
	{
		// Implementation (No changes)
	}

	void PersonalVehicles::RegisterGaragesImpl()
	{
		// Implementation (No changes)
	}

}
