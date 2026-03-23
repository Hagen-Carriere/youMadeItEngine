#include "actorDB.h"
actorDB* actorDB::instance = nullptr;

void actorDB::initActors(rapidjson::Document* in, bool tiledIn, SDL_Renderer* rendererIn, SceneDB::camera* camIn, lua_State* lIn)
{
	lua_state = lIn;
	actorDB::instance = this;

	luabridge::getGlobalNamespace(lua_state)
		.beginClass<actorDB::Actor>("Actor")
		.addFunction("GetName", &actorDB::Actor::GetName)
		.addFunction("GetID", &actorDB::Actor::GetID)
		.addFunction("GetGID", &actorDB::Actor::GetGID)
		.addFunction("GetComponentByKey", &actorDB::Actor::GetComponentByKey)
		.addFunction("GetComponent", &actorDB::Actor::GetComponent)
		.addFunction("GetComponents", &actorDB::Actor::GetComponents)
		.addFunction("AddComponent", &actorDB::Actor::AddComponentLua)
		.addFunction("RemoveComponent", &actorDB::Actor::RemoveComponent)
		.endClass()
		.beginNamespace("Actor")
		.addFunction("Find", &actorDB::Lua_Find)
		.addFunction("FindAll", &actorDB::Lua_FindAll)
		.addFunction("Instantiate", &actorDB::Instantiate)
		.addFunction("Destroy", &actorDB::Destroy)
		.endNamespace();

	// Clear existing data structures
	destroyAllActors();
	renderer = rendererIn;
	camPointer = camIn;
	componentManager.init(lIn);

	const auto& actorsArray = (*in)["actors"];
	actorsList.reserve(actorsArray.Size() + actorsSaved.size());

	for (auto& actor : actorsSaved)
	{
		actorsList.push_back(std::move(actor));
	}
	actorsSaved.clear();


	if (tiledIn) {
		// Initialize the templateDB with the tiled map
		std::vector<int> actorLayers = tiledManager.init(in, camIn);
		if (!actorLayers.empty()) {
			for (int i = 0; i < actorLayers.size(); i++) {
				const auto& layer = (*in)["layers"][actorLayers[i]];
				const auto& objectsArray = layer["objects"];
				for (rapidjson::SizeType j = 0; j < objectsArray.Size(); ++j) {
					auto newActor = std::make_unique<Actor>(actorIDCount++);
					Actor* actorPtr = newActor.get(); // Safe and stable pointer
					actorsList.push_back(std::move(newActor));
					actorIDCount++;
					const auto& actorData = objectsArray[j];
					initActor(actorPtr, actorData, true);
				}
				
			}

		}
	}
	else {
		if (!in || !in->IsObject() || !(*in)["actors"].IsArray()) {
			return;
		}

		for (rapidjson::SizeType i = 0; i < actorsArray.Size(); ++i) {
			auto newActor = std::make_unique<Actor>(actorIDCount++);
			Actor* actorPtr = newActor.get(); // Safe and stable pointer
			actorsList.push_back(std::move(newActor));
			actorIDCount++;
			const auto& actorData = actorsArray[i];
			initActor(actorPtr, actorData, false);
		}
	}


	// After all actors are fully initialized
	for (const auto& actor : actorsList) {
		for (const std::string& key : actor->onStartComponents) {
			componentManager.callFunction(actor->components[key], "OnStart", actor->GetName());
			actor->components[key].hasStarted = true;
		}
	}
}

void actorDB::updateAllActors() {
	// Apply deferred components
	for (auto& actor : actorsList) {
		for (auto& [key, info] : actor->pendingComponents) {
			actor->components.emplace(key, info);
			actor->keys.push_back(key);
			if ((*info.luaRef)["OnStart"].isFunction()) {
				actor->onStartComponents.push_back(key);
			}
			if ((*info.luaRef)["OnUpdate"].isFunction()) {
				actor->onUpdateComponents.push_back(key);
			}
			if ((*info.luaRef)["OnLateUpdate"].isFunction()) {
				actor->onLateUpdateComponents.push_back(key);
			}

		}
		if (!actor->pendingComponents.empty()) {
			std::sort(actor->keys.begin(), actor->keys.end());
			std::sort(actor->onStartComponents.begin(), actor->onStartComponents.end());
			std::sort(actor->onUpdateComponents.begin(), actor->onUpdateComponents.end());
			std::sort(actor->onLateUpdateComponents.begin(), actor->onLateUpdateComponents.end());
			actor->pendingComponents.clear();
		}

	}

	for (auto& actor : actorsList) {
		for (const auto& key : actor->onStartComponents) {
			auto& comp = actor->components[key];
			componentManager.callFunction(comp, "OnStart", actor->GetName());
			comp.hasStarted = true;
		}
		actor->onStartComponents.clear(); // Clear after calling OnStart
	}

	// First pass: OnUpdate
	for (auto& actor : actorsList) {
		for (const auto& key : actor->onUpdateComponents) {
			auto& comp = actor->components[key];
			componentManager.callFunction(comp, "OnUpdate", actor->GetName());
		}
	}
	// Second pass: OnLateUpdate
	for (auto& actor : actorsList) {
		for (const auto& key : actor->onLateUpdateComponents) {
			auto& comp = actor->components[key];
			componentManager.callFunction(comp, "OnLateUpdate", actor->GetName());
		}
	}

	// After OnLateUpdate pass
	for (auto& actor : actorsList) {
		for (const std::string& key : actor->componentsToRemove) {
			// Call OnDestroy if it exists
			auto& comp = actor->components[key];
			if ((*comp.luaRef)["OnDestroy"].isFunction()) {
				componentManager.callFunction(comp, "OnDestroy", actor->GetName());
			}

			//make lua table nil
			actor->components[key].luaRef->operator=(luabridge::LuaRef(lua_state));

			//erase key from typeToKeys
			auto typeItr = actor->typeToKeys.find(actor->components[key].type);
			if (typeItr != actor->typeToKeys.end()) {
				auto& keys = typeItr->second;
				keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
				if (keys.empty()) {
					actor->typeToKeys.erase(typeItr);
				}
			}

			//erase key from onStart if it is in there
			actor->onStartComponents.erase(std::remove(actor->onStartComponents.begin(), actor->onStartComponents.end(), key), actor->onStartComponents.end());
			actor->onUpdateComponents.erase(std::remove(actor->onUpdateComponents.begin(), actor->onUpdateComponents.end(), key), actor->onUpdateComponents.end());
			actor->onLateUpdateComponents.erase(std::remove(actor->onLateUpdateComponents.begin(), actor->onLateUpdateComponents.end(), key), actor->onLateUpdateComponents.end());

			actor->components.erase(key);
			actor->keys.erase(std::remove(actor->keys.begin(), actor->keys.end(), key), actor->keys.end());
		}
		actor->componentsToRemove.clear(); // clear for next frame
	}



	// Commit pending actors
	for (auto& actor : actorsToAdd) {
		actorsList.push_back(std::move(actor));
	}
	actorsToAdd.clear();

	// Remove destroyed actors
	if (!actorsToDestroy.empty()) {
		for (const auto& actor : actorsList) {
			if (std::find(actorsToDestroy.begin(), actorsToDestroy.end(), actor->id) != actorsToDestroy.end()) {
				std::vector<std::string> sortedKeys = actor->keys;
				std::sort(sortedKeys.begin(), sortedKeys.end());
				for (const std::string& key : sortedKeys) {
					auto& comp = actor->components[key];
					if ((*comp.luaRef)["OnDestroy"].isFunction()) {
						componentManager.callFunction(comp, "OnDestroy", actor->GetName());
					}
				}
			}
		}

		actorsList.erase(std::remove_if(actorsList.begin(), actorsList.end(),
			[&](const std::unique_ptr<Actor>& a) {
				return std::find(actorsToDestroy.begin(), actorsToDestroy.end(), a->id) != actorsToDestroy.end();
			}),
			actorsList.end());	
		actorsToDestroy.clear();
	}

	componentManager.world->Step(1.0f / 60.0f, 8, 3);
}



void actorDB::initActor(Actor* actor, const rapidjson::Value& actorData, bool tiled)
{

	const auto s = actorData.FindMember("template");
	const auto actComponentsTiled = actorData.FindMember("properties");
	if (tiled) {
		//if (actComponentsTiled != actorData.MemberEnd()) {
		//	//iterate over the properties and find if name property holds template name
		//	for (auto itr = actComponentsTiled->value.MemberBegin(); itr != actComponentsTiled->value.MemberEnd(); ++itr) {
		//		if (itr->value["name"] == "template") {
		//			actor->templateName = itr->value.GetString();
		//			break;
		//		}
		//	}
		//}
	}else if (s != actorData.MemberEnd()) {
		actor->templateName = s->value.GetString();
	}

	rapidjson::Document* templateActor = nullptr;
	bool templateExists = false;
	if (!actor->templateName.empty()) {
		// Get the template
		templateActor = getTemplate(actor->templateName);
		templateExists = templateActor != nullptr;
	}

	if (tiled) {
		actor->name = actorData["name"].GetString();
		actor->gid = actorData["gid"].GetInt();
	}
	else if(!tiled) {
		const auto s2 = actorData.FindMember("name");
		if (s2 != actorData.MemberEnd()) {
			actor->name = s2->value.GetString();
		}
	}
	else if (templateExists) {
		const auto s3 = templateActor->FindMember("name");
		if (s3 != templateActor->MemberEnd()) {
			actor->name = s3->value.GetString();
		}
	}


	actor->lua_state = lua_state;


	const auto actComponents = actorData.FindMember("components");
	if (templateExists) {
		// No components? Early out
		const auto templateComponents = (*templateActor).FindMember("components");
		if ((templateComponents == (*templateActor).MemberEnd()) && actComponents == actorData.MemberEnd() && actComponentsTiled == actorData.MemberEnd()) {
			return;
		}

		const auto& componentObject = (*templateActor)["components"];
		for (auto itr = componentObject.MemberBegin(); itr != componentObject.MemberEnd(); ++itr) {
			if (!itr->value.IsObject()) continue;

			const std::string key = itr->name.GetString();
			const rapidjson::Value& componentData = itr->value;

			std::string type;
			const auto typeItr = componentData.FindMember("type");
			if (typeItr != componentData.MemberEnd()) {
				type = typeItr->value.GetString();
			}

			actor->AddComponent(type, key, componentData, true);
			if ((*actor->components[key].luaRef)["OnStart"].isFunction()) {
				actor->onStartComponents.push_back(key);
			}
			if ((*actor->components[key].luaRef)["OnUpdate"].isFunction()) {
				actor->onUpdateComponents.push_back(key);
			}
			if ((*actor->components[key].luaRef)["OnLateUpdate"].isFunction()) {
				actor->onLateUpdateComponents.push_back(key);
			}

			if (std::find(actor->keys.begin(), actor->keys.end(), key) == actor->keys.end()) {
				actor->keys.push_back(key);
			}
		}
	}

	// No components? Early out
	if (actComponents == actorData.MemberEnd() && actComponentsTiled == actorData.MemberEnd()) {
		return;
	}

	if (tiled) {
		const auto& componentArray = actorData["properties"];
		for (rapidjson::SizeType i = 0; i < componentArray.Size(); ++i) {
			const auto& prop = componentArray[i];
			const std::string type = prop["name"].GetString();   // Component type
			const std::string rawValue = prop["value"].GetString();

			std::stringstream ss(rawValue);
			std::vector<std::string> componentDataVector;
			std::string line;
			while (std::getline(ss, line)) {
				componentDataVector.push_back(line);
			}

			if (componentDataVector.empty()) {
				std::cout << "Warning: No data for component " << type << std::endl;
				continue;
			}

			const std::string key = componentDataVector[0];

			actor->AddComponent(type, key, componentDataVector, true);
			if ((*actor->components[key].luaRef)["OnStart"].isFunction()) {
				actor->onStartComponents.push_back(key);
			}
			if ((*actor->components[key].luaRef)["OnUpdate"].isFunction()) {
				actor->onUpdateComponents.push_back(key);
			}
			if ((*actor->components[key].luaRef)["OnLateUpdate"].isFunction()) {
				actor->onLateUpdateComponents.push_back(key);
			}
			if (std::find(actor->keys.begin(), actor->keys.end(), key) == actor->keys.end()) {
				actor->keys.push_back(key);
			}
		}

	}
	else {
		// next iterate over the actorData and overwrite the components
		const auto& componentObject2 = actorData["components"];
		for (auto itr = componentObject2.MemberBegin(); itr != componentObject2.MemberEnd(); ++itr) {
			if (!itr->value.IsObject()) continue;

			const std::string key = itr->name.GetString();
			const rapidjson::Value& componentData = itr->value;

			std::string type;
			const auto typeItr = componentData.FindMember("type");
			if (typeItr != componentData.MemberEnd()) {
				type = typeItr->value.GetString();
			}

			//check if component exists
			auto compItr = actor->components.find(key);
			if (compItr != actor->components.end()) {
				//update component
				componentManager.updateInstance(compItr->second.luaRef, componentData, type);
			}
			else {
				actor->AddComponent(type, key, componentData, true);
				if ((*actor->components[key].luaRef)["OnStart"].isFunction()) {
					actor->onStartComponents.push_back(key);
				}
				if ((*actor->components[key].luaRef)["OnUpdate"].isFunction()) {
					actor->onUpdateComponents.push_back(key);
				}
				if ((*actor->components[key].luaRef)["OnLateUpdate"].isFunction()) {
					actor->onLateUpdateComponents.push_back(key);
				}

				if (std::find(actor->keys.begin(), actor->keys.end(), key) == actor->keys.end()) {
					actor->keys.push_back(key);
				}

			}
		}
	}

	std::sort(actor->keys.begin(), actor->keys.end());
	std::sort(actor->onStartComponents.begin(), actor->onStartComponents.end());
	std::sort(actor->onUpdateComponents.begin(), actor->onUpdateComponents.end());
	std::sort(actor->onLateUpdateComponents.begin(), actor->onLateUpdateComponents.end());
}

rapidjson::Document* actorDB::getTemplate(const std::string& templateName)
{
	//check if template exists
	if (templates.find(templateName) == templates.end()) {
		//load template
		rapidjson::Document templateDoc = templateDB.getTemplate(templateName);
		if (!templateDoc.IsObject()) {
			return nullptr;
		}

		templates.emplace(templateName, std::move(templateDoc));
	}
	return &templates[templateName];
}

actorDB::Actor* actorDB::Instantiate(const std::string& templateName)
{
	// Create actor on the heap
	auto newActor = std::make_unique<Actor>(actorIDCount++);
	newActor->templateName = templateName;

	Actor* actorPtr = newActor.get(); // Safe pointer to return
	// Check if template exists
	rapidjson::Document* templateActor = instance->getTemplate(templateName);

	instance->initActor(actorPtr, *templateActor, false);

	// Queue for next-frame addition
	instance->actorsToAdd.push_back(std::move(newActor));

	return actorPtr;
}

void actorDB::Destroy(actorDB::Actor* actor)
{
	if (!actor) return;
	// Disable all components immediately
	for (auto& [key, compInfo] : actor->components)
	{
		compInfo.enabled = false;
		(*compInfo.luaRef)["enabled"] = false;
	}

	// Mark actor ID for removal at end of frame
	instance->actorsToDestroy.push_back(actor->id);
}

std::vector<actorDB::Actor*> actorDB::FindAll(const std::string& name)
{
	std::vector<actorDB::Actor*> result;
	for (auto& actor : instance->actorsList)
	{
		if (actor->name == name)
		{
			result.push_back(actor.get());
		}
	}

	// Also check pending actors
	for (auto& actor : instance->actorsToAdd)
	{
		if (actor->name == name && !isPendingDestruction(actor->id))
		{
			result.push_back(actor.get());
		}
	}

	return result;
}

actorDB::Actor* actorDB::Find(const std::string& name)
{
	for (auto& actor : instance->actorsList)
	{
		if (actor->name == name && !isPendingDestruction(actor->id))
		{
			return actor.get();
		}
	}

	for (auto& actor : instance->actorsToAdd)
	{
		if (actor->name == name && !isPendingDestruction(actor->id))
		{
			return actor.get();
		}
	}

	return nullptr;
}

actorDB::Actor* actorDB::Lua_Find(const std::string& name)
{
	// assuming lua_state is always set before use
	return Find(name);
}

luabridge::LuaRef actorDB::Lua_FindAll(const std::string& name)
{
	lua_State* L = Actor::lua_state;
	luabridge::LuaRef table = luabridge::newTable(L);
	int index = 1;
	for (actorDB::Actor* actor : FindAll(name)) {
		table[index++] = actor;
	}
	if (index == 1) {
		return luabridge::LuaRef(instance->lua_state);
	}
	return table;
}

void actorDB::destroyAllActors()
{
	for (auto& actor : actorsList)
	{
		if (actor->dontDestroyOnLoad)
		{
			actorsSaved.push_back(std::move(actor));
		}
		else
		{
			// Call OnDestroy for all components
			for (const auto& key : actor->keys) {
				auto& comp = actor->components[key];
				if ((*comp.luaRef)["OnDestroy"].isFunction()) {
					componentManager.callFunction(comp, "OnDestroy", actor->GetName());
				}
			}
		}
	}

	actorsList.clear();
}


std::string actorDB::Actor::GetName() const
{
	return name;
}

int actorDB::Actor::GetID() const
{
	return id;
}

luabridge::LuaRef actorDB::Actor::GetComponentByKey(const std::string& key)
{
	if (components.find(key) != components.end()) {
		return *components[key].luaRef;
	}
	return luabridge::LuaRef(lua_state);
}

luabridge::LuaRef actorDB::Actor::GetComponent(const std::string& typeName)
{
	auto it = typeToKeys.find(typeName);
	if (it != typeToKeys.end()) {
		for (auto keys : it->second)
		{
			if (components.find(keys) != components.end() && components[keys].enabled) {
				return *components[keys].luaRef;
			}
		}
	}
	return luabridge::LuaRef(lua_state);
}


luabridge::LuaRef actorDB::Actor::GetComponents(const std::string& typeName)
{
	luabridge::LuaRef table = luabridge::newTable(lua_state);
	auto it = typeToKeys.find(typeName);
	if (it != typeToKeys.end()) {
		int index = 1;

		for (auto keys : it->second)
		{
			if (components[keys].enabled) {
				table[index++] = *components[keys].luaRef;
			}
		}
	}
	return table;
}

luabridge::LuaRef actorDB::Actor::AddComponent(const std::string& type, const std::string& key, const rapidjson::Value& props, bool immediate)
{
	std::shared_ptr<luabridge::LuaRef> component = nullptr;
	if (type == "Rigidbody" || type == "ParticleSystem")
	{
		component = actorDB::instance->createInstance(type, key, props, this, actorDB::instance->componentManager.world);
	}
	else {
		component = actorDB::instance->componentManager.createInstance(type, key, props);
		(*component)["actor"] = this;

	}

	ComponentDB::ComponentInfo info{ type, component };
	typeToKeys[type].push_back(key);
	if (immediate)
	{
		components.emplace(key, info);
		keys.push_back(key);
	}
	else
	{
		pendingComponents.emplace_back(key, info);
	}
	return *component;
}

luabridge::LuaRef actorDB::Actor::AddComponent(const std::string& typeName, const std::string& key, const std::vector<std::string> props, bool immediate)
{
	std::shared_ptr<luabridge::LuaRef> component = nullptr;
	if (typeName == "Rigidbody" || typeName == "ParticleSystem")
	{
		component = actorDB::instance->createInstance(typeName, key, props, this, actorDB::instance->componentManager.world);
	}
	else {
		component = actorDB::instance->componentManager.createInstance(typeName, key, props);
		(*component)["actor"] = this;

	}

	ComponentDB::ComponentInfo info{ typeName, component };
	typeToKeys[typeName].push_back(key);
	if (immediate)
	{
		components.emplace(key, info);
		keys.push_back(key);
	}
	else
	{
		pendingComponents.emplace_back(key, info);
	}
	return *component;
}

luabridge::LuaRef actorDB::Actor::AddComponentLua(const std::string& type)
{
	std::string key = "r" + std::to_string(ComponentDB::runtime_component_counter++);

	// Dummy JSON value
	rapidjson::Document dummy;
	dummy.SetObject();

	return AddComponent(type, key, dummy, false);
}

void actorDB::Actor::RemoveComponent(const luabridge::LuaRef& componentRef)
{
	for (auto& [key, compInfo] : components)
	{
		if (compInfo.luaRef && *compInfo.luaRef == componentRef)
		{
			compInfo.enabled = false;
			if(!compInfo.luaRef->isUserdata())(*compInfo.luaRef)["enabled"] = false;
			
			componentsToRemove.push_back(key);
		}
	}
}

bool actorDB::isPendingDestruction(int actorID)
{
	return std::find(instance->actorsToDestroy.begin(), instance->actorsToDestroy.end(), actorID) != instance->actorsToDestroy.end();
}

std::shared_ptr<luabridge::LuaRef> actorDB::createInstance(const std::string& type, const std::string& key, const rapidjson::Value& properties, actorDB::Actor* aIn, b2World* world)
{
	if (type == "Rigidbody") {
		Rigidbody* new_component = new Rigidbody();

		for (auto itr = properties.MemberBegin(); itr != properties.MemberEnd(); ++itr) {
			const std::string name = itr->name.GetString();

			if (name == "x") new_component->x = itr->value.GetFloat();
			else if (name == "y") new_component->y = itr->value.GetFloat();
			else if (name == "body_type") new_component->body_type = itr->value.GetString();
			else if (name == "gravity_scale") new_component->gravity_scale = itr->value.GetFloat();
			else if (name == "density") new_component->density = itr->value.GetFloat();
			else if (name == "angular_friction") new_component->angular_friction = itr->value.GetFloat();
			else if (name == "rotation") new_component->rotation = itr->value.GetFloat();
			else if (name == "has_collider") new_component->has_collider = itr->value.GetBool();
			else if (name == "has_trigger") new_component->has_trigger = itr->value.GetBool();
			else if (name == "collider_type") new_component->collider_type = itr->value.GetString();
			else if (name == "width") new_component->width = itr->value.GetFloat();
			else if (name == "height") new_component->height = itr->value.GetFloat();
			else if (name == "radius") new_component->radius = itr->value.GetFloat();
			else if (name == "friction") new_component->friction = itr->value.GetFloat();
			else if (name == "bounciness") new_component->bounciness = itr->value.GetFloat();
			else if (name == "trigger_height") new_component->trigger_height= itr->value.GetFloat();
			else if (name == "trigger_width") new_component->trigger_width = itr->value.GetFloat();
			else if (name == "trigger_radius") new_component->trigger_radius = itr->value.GetFloat();
			else if (name == "trigger_type") new_component->trigger_type = itr->value.GetString();

		}


		new_component->key = key;
		new_component->actor = aIn;
		new_component->enabled = true;
		//new_component->Init(world);

		luabridge::LuaRef ref(lua_state, new_component);

		return std::make_shared<luabridge::LuaRef>(ref);
	}
	else if (type == "ParticleSystem") {
		ParticleSystem* new_component = new ParticleSystem();

		for (auto itr = properties.MemberBegin(); itr != properties.MemberEnd(); ++itr) {
			const std::string name = itr->name.GetString();
			const auto& value = itr->value;

			if (name == "x") new_component->x = value.GetFloat();
			else if (name == "y") new_component->y = value.GetFloat();
			else if (name == "frames_between_bursts") new_component->frames_between_bursts = std::max(1, value.GetInt());
			else if (name == "burst_quantity") new_component->burst_quantity = std::max(1, value.GetInt());
			else if (name == "start_scale_min") new_component->start_scale_min = value.GetFloat();
			else if (name == "start_scale_max") new_component->start_scale_max = value.GetFloat();
			else if (name == "rotation_min") new_component->rotation_min = value.GetFloat();
			else if (name == "rotation_max") new_component->rotation_max = value.GetFloat();
			else if (name == "start_color_r") new_component->start_color_r = value.GetInt();
			else if (name == "start_color_g") new_component->start_color_g = value.GetInt();
			else if (name == "start_color_b") new_component->start_color_b = value.GetInt();
			else if (name == "start_color_a") new_component->start_color_a = value.GetInt();
			else if (name == "emit_radius_min") new_component->emit_radius_min = value.GetFloat();
			else if (name == "emit_radius_max") new_component->emit_radius_max = value.GetFloat();
			else if (name == "emit_angle_min") new_component->emit_angle_min = value.GetFloat();
			else if (name == "emit_angle_max") new_component->emit_angle_max = value.GetFloat();
			else if (name == "image") new_component->image = value.GetString();
			else if (name == "sorting_order") new_component->sorting_order = value.GetInt();
			else if (name == "duration_frames") new_component->duration_frames = std::max(1, value.GetInt());
			else if (name == "start_speed_min") new_component->start_speed_min = value.GetFloat();
			else if (name == "start_speed_max") new_component->start_speed_max = value.GetFloat();
			else if (name == "rotation_speed_min") new_component->rotation_speed_min = value.GetFloat();
			else if (name == "rotation_speed_max") new_component->rotation_speed_max = value.GetFloat();
			else if (name == "gravity_scale_x") new_component->gravity_scale_x = value.GetFloat();
			else if (name == "gravity_scale_y") new_component->gravity_scale_y = value.GetFloat();
			else if (name == "drag_factor") new_component->drag_factor = value.GetFloat();
			else if (name == "angular_drag_factor") new_component->angular_drag_factor = value.GetFloat();
			else if (name == "end_scale") new_component->end_scale.emplace(value.GetFloat());
			else if (name == "end_color_r") new_component->end_color_r.emplace(value.GetInt());
			else if (name == "end_color_g") new_component->end_color_g.emplace(value.GetInt());
			else if (name == "end_color_b") new_component->end_color_b.emplace(value.GetInt());
			else if (name == "end_color_a") new_component->end_color_a.emplace(value.GetInt());
				
			
		}

		// Clamp and setup random engines
		new_component->angleGen = RandomEngine(new_component->emit_angle_min, new_component->emit_angle_max, 298);
		new_component->radiusGen = RandomEngine(new_component->emit_radius_min, new_component->emit_radius_max, 404);
		new_component->rotationGen = RandomEngine(new_component->rotation_min, new_component->rotation_max, 440);
		new_component->scaleGen = RandomEngine(new_component->start_scale_min, new_component->start_scale_max, 494);
		new_component->speedGen = RandomEngine(new_component->start_speed_min, new_component->start_speed_max, 498);
		new_component->rotationSpeedGen = RandomEngine(new_component->rotation_speed_min, new_component->rotation_speed_max, 305);


		new_component->key = key;
		new_component->actor = aIn;
		new_component->enabled = true;

		luabridge::LuaRef ref(lua_state, new_component);
		return std::make_shared<luabridge::LuaRef>(ref);
	}

    return nullptr;
}

std::shared_ptr<luabridge::LuaRef> actorDB::createInstance(const std::string& type, const std::string& key, const std::vector<std::string> props, actorDB::Actor* aIn, b2World* world)
{
	if (type == "Rigidbody") {
		Rigidbody* new_component = new Rigidbody();

		for (auto prop : props) {
			std::string type, name, value;
			instance->componentManager.parseTypedKeyValue(prop, type, name, value);

			if (name == "x") new_component->x = std::stof(value);
			else if (name == "y") new_component->y = std::stof(value);
			else if (name == "body_type") new_component->body_type = value;
			else if (name == "gravity_scale") new_component->gravity_scale = std::stof(value);
			else if (name == "density") new_component->density = std::stof(value);
			else if (name == "angular_friction") new_component->angular_friction = std::stof(value);
			else if (name == "rotation") new_component->rotation = std::stof(value);
			else if (name == "has_collider") new_component->has_collider = (value == "true");
			else if (name == "has_trigger") new_component->has_trigger = (value == "true");
			else if (name == "collider_type") new_component->collider_type = value;
			else if (name == "width") new_component->width = std::stof(value);
			else if (name == "height") new_component->height = std::stof(value);
			else if (name == "radius") new_component->radius = std::stof(value);
			else if (name == "friction") new_component->friction = std::stof(value);
			else if (name == "bounciness") new_component->bounciness = std::stof(value);
			else if (name == "trigger_height") new_component->trigger_height = std::stof(value);
			else if (name == "trigger_width") new_component->trigger_width = std::stof(value);
			else if (name == "trigger_radius") new_component->trigger_radius = std::stof(value);
			else if (name == "trigger_type") new_component->trigger_type = value;

		}


		new_component->key = key;
		new_component->actor = aIn;
		new_component->enabled = true;
		//new_component->Init(world);

		luabridge::LuaRef ref(lua_state, new_component);

		return std::make_shared<luabridge::LuaRef>(ref);
	}
	else if (type == "ParticleSystem") {
		ParticleSystem* new_component = new ParticleSystem();

		for (auto prop : props) {
			std::string type, name, value;
			instance->componentManager.parseTypedKeyValue(prop, type, name, value);

			if (name == "x") new_component->x = std::stof(value);
			else if (name == "y") new_component->y = std::stof(value);
			else if (name == "frames_between_bursts") new_component->frames_between_bursts = std::max(1, std::stoi(value));
			else if (name == "burst_quantity") new_component->burst_quantity = std::max(1, std::stoi(value));
			else if (name == "start_scale_min") new_component->start_scale_min = std::stof(value);
			else if (name == "start_scale_max") new_component->start_scale_max = std::stof(value);
			else if (name == "rotation_min") new_component->rotation_min = std::stof(value);
			else if (name == "rotation_max") new_component->rotation_max = std::stof(value);
			else if (name == "start_color_r") new_component->start_color_r = std::stoi(value);
			else if (name == "start_color_g") new_component->start_color_g = std::stoi(value);
			else if (name == "start_color_b") new_component->start_color_b = std::stoi(value);
			else if (name == "start_color_a") new_component->start_color_a = std::stoi(value);
			else if (name == "emit_radius_min") new_component->emit_radius_min = std::stof(value);
			else if (name == "emit_radius_max") new_component->emit_radius_max = std::stof(value);
			else if (name == "emit_angle_min") new_component->emit_angle_min = std::stof(value);
			else if (name == "emit_angle_max") new_component->emit_angle_max = std::stof(value);
			else if (name == "image") new_component->image = value;
			else if (name == "sorting_order") new_component->sorting_order = std::stoi(value);
			else if (name == "duration_frames") new_component->duration_frames = std::max(1, std::stoi(value));
			else if (name == "start_speed_min") new_component->start_speed_min = std::stof(value);
			else if (name == "start_speed_max") new_component->start_speed_max = std::stof(value);
			else if (name == "rotation_speed_min") new_component->rotation_speed_min = std::stof(value);
			else if (name == "rotation_speed_max") new_component->rotation_speed_max = std::stof(value);
			else if (name == "gravity_scale_x") new_component->gravity_scale_x = std::stof(value);
			else if (name == "gravity_scale_y") new_component->gravity_scale_y = std::stof(value);
			else if (name == "drag_factor") new_component->drag_factor = std::stof(value);
			else if (name == "angular_drag_factor") new_component->angular_drag_factor = std::stof(value);
			else if (name == "end_scale") new_component->end_scale.emplace(std::stof(value));
			else if (name == "end_color_r") new_component->end_color_r.emplace(std::stoi(value));
			else if (name == "end_color_g") new_component->end_color_g.emplace(std::stoi(value));
			else if (name == "end_color_b") new_component->end_color_b.emplace(std::stoi(value));
			else if (name == "end_color_a") new_component->end_color_a.emplace(std::stoi(value));


		}

		// Clamp and setup random engines
		new_component->angleGen = RandomEngine(new_component->emit_angle_min, new_component->emit_angle_max, 298);
		new_component->radiusGen = RandomEngine(new_component->emit_radius_min, new_component->emit_radius_max, 404);
		new_component->rotationGen = RandomEngine(new_component->rotation_min, new_component->rotation_max, 440);
		new_component->scaleGen = RandomEngine(new_component->start_scale_min, new_component->start_scale_max, 494);
		new_component->speedGen = RandomEngine(new_component->start_speed_min, new_component->start_speed_max, 498);
		new_component->rotationSpeedGen = RandomEngine(new_component->rotation_speed_min, new_component->rotation_speed_max, 305);


		new_component->key = key;
		new_component->actor = aIn;
		new_component->enabled = true;

		luabridge::LuaRef ref(lua_state, new_component);
		return std::make_shared<luabridge::LuaRef>(ref);
	}

	return nullptr;
}


void ComponentDB::init(lua_State* state)
{
	lua_state = state;

	// Register Debug.Log for Lua usage
	luabridge::getGlobalNamespace(lua_state)
		.beginNamespace("Debug")
		.addFunction("Log", &ComponentDB::debugLog)
		.endNamespace()
		.beginClass<Rigidbody>("Rigidbody")
		.addConstructor<void(*)()>()
		.addProperty("x", &Rigidbody::x)
		.addProperty("y", &Rigidbody::y)
		.addProperty("body_type", &Rigidbody::body_type)
		.addProperty("gravity_scale", &Rigidbody::gravity_scale)
		.addProperty("density", &Rigidbody::density)
		.addProperty("angular_friction", &Rigidbody::angular_friction)
		.addProperty("rotation", &Rigidbody::rotation)
		.addProperty("has_collider", &Rigidbody::has_collider)
		.addProperty("has_trigger", &Rigidbody::has_trigger)
		.addProperty("actor", &Rigidbody::actor)
		.addProperty("collider_type", &Rigidbody::collider_type)
		.addProperty("width", &Rigidbody::width)
		.addProperty("height", &Rigidbody::height)
		.addProperty("radius", &Rigidbody::radius)
		.addProperty("friction", &Rigidbody::friction)
		.addProperty("bounciness", &Rigidbody::bounciness)
		.addFunction("OnStart", &Rigidbody::OnStart)
		.addFunction("GetPosition", &Rigidbody::GetPosition)
		.addFunction("GetRotation", &Rigidbody::GetRotation)
		.addFunction("AddForce", &Rigidbody::AddForce)
		.addFunction("SetVelocity", &Rigidbody::SetVelocity)
		.addFunction("SetPosition", &Rigidbody::SetPosition)
		.addFunction("SetRotation", &Rigidbody::SetRotation)
		.addFunction("SetAngularVelocity", &Rigidbody::SetAngularVelocity)
		.addFunction("SetGravityScale", &Rigidbody::SetGravityScale)
		.addFunction("SetUpDirection", &Rigidbody::SetUpDirection)
		.addFunction("SetRightDirection", &Rigidbody::SetRightDirection)
		.addFunction("GetVelocity", &Rigidbody::GetVelocity)
		.addFunction("GetAngularVelocity", &Rigidbody::GetAngularVelocity)
		.addFunction("GetGravityScale", &Rigidbody::GetGravityScale)
		.addFunction("GetUpDirection", &Rigidbody::GetUpDirection)
		.addFunction("GetRightDirection", &Rigidbody::GetRightDirection)
		.addFunction("OnDestroy", &Rigidbody::OnDestroy)
		.endClass()
		.beginClass<b2Vec2>("Vector2")
		.addConstructor<void(*) (float, float)>()
		.addProperty("x", &b2Vec2::x)
		.addProperty("y", &b2Vec2::y)
		.addFunction("Normalize", &b2Vec2::Normalize)
		.addFunction("Length", &b2Vec2::Length)
		.addFunction("__add", &b2Vec2::operator_add)
		.addFunction("__sub", &b2Vec2::operator_sub)
		.addFunction("__mul", &b2Vec2::operator_mul)
		.endClass()
		.beginNamespace("Vector2")
		.addFunction("Distance", &Rigidbody::Vector2_Distance)
		.addFunction("Dot", static_cast<float (*)(const b2Vec2&, const b2Vec2&)>(&Rigidbody::Vector2_Dot))
		.endNamespace().beginClass<Collision>("Collision")
		.addProperty("other", &Collision::other)
		.addProperty("point", &Collision::point)
		.addProperty("normal", &Collision::normal)
		.addProperty("relative_velocity", &Collision::relative_velocity)
		.endClass()
		.beginNamespace("Physics")
		.addFunction("Raycast", &Physics_Raycast)
		.addFunction("RaycastAll", &Physics_RaycastAll)
		.endNamespace()
		.beginClass<ParticleSystem>("ParticleSystem")
		.addConstructor<void(*)()>()
		.addProperty("actor", &ParticleSystem::actor)
		.addProperty("enabled", &ParticleSystem::enabled)
		.addProperty("x", &ParticleSystem::x)
		.addProperty("y", &ParticleSystem::y)
		.addProperty("start_color_r", &ParticleSystem::start_color_r)
		.addProperty("start_color_g", &ParticleSystem::start_color_g)
		.addProperty("start_color_b", &ParticleSystem::start_color_b)
		.addProperty("start_color_a", &ParticleSystem::start_color_a)
		.addFunction("OnStart", &ParticleSystem::OnStart)
		.addFunction("OnUpdate", &ParticleSystem::OnUpdate)
		.addFunction("OnDestroy", &ParticleSystem::OnDestroy)
		.addFunction("Play", &ParticleSystem::Play)
		.addFunction("Stop", &ParticleSystem::Stop)
		.addFunction("Burst", &ParticleSystem::Burst)
		.endClass();



	world = new b2World(b2Vec2(0.0f, 9.8f));
	contactListener = new ContactListener();
	world->SetContactListener(contactListener);



}


std::shared_ptr<luabridge::LuaRef> ComponentDB::loadComponent(std::string type)
{
	if (componentScripts.find(type) == componentScripts.end())
	{
		std::string scriptPath = "resources/component_types/" + type + ".lua";
		if (!std::filesystem::exists(scriptPath))
		{
			std::cout << "error: failed to locate component " << type;
			exit(0);
		}

		if (luaL_dofile(lua_state, scriptPath.c_str()) != LUA_OK)
		{
			std::cout << "problem with lua file " << type;
			exit(0);
		}
		luabridge::LuaRef componentTable = luabridge::getGlobal(lua_state, type.c_str());
		componentScripts[type] = std::make_shared<luabridge::LuaRef>(componentTable);
	}
	return componentScripts[type];
}

std::shared_ptr<luabridge::LuaRef> ComponentDB::createInstance(const std::string& type, const std::string& key, const rapidjson::Value& properties)
{
	std::shared_ptr<luabridge::LuaRef> baseComponent = loadComponent(type);
	if (!baseComponent || !baseComponent->isTable())
	{
		std::cout << "error: failed to locate component " << type;
		exit(0);
	}

	luabridge::LuaRef instance = luabridge::newTable(lua_state);
	establishInheritance(instance, *baseComponent);
	instance["key"] = key;
	instance["enabled"] = true;

	for (auto itr = properties.MemberBegin(); itr != properties.MemberEnd(); ++itr) {
		if (itr->value.IsString())
			instance[itr->name.GetString()] = itr->value.GetString();
		else if (itr->value.IsInt())
			instance[itr->name.GetString()] = itr->value.GetInt();
		else if (itr->value.IsFloat())
			instance[itr->name.GetString()] = itr->value.GetFloat();
		else if (itr->value.IsBool())
			instance[itr->name.GetString()] = itr->value.GetBool();
	}

	return std::make_shared<luabridge::LuaRef>(instance);
}

std::shared_ptr<luabridge::LuaRef> ComponentDB::createInstance(const std::string& type, const std::string& key, const std::vector<std::string> props)
{
	std::shared_ptr<luabridge::LuaRef> baseComponent = loadComponent(type);
	if (!baseComponent || !baseComponent->isTable())
	{
		std::cout << "error: failed to locate component " << type;
		exit(0);
	}

	luabridge::LuaRef instance = luabridge::newTable(lua_state);
	establishInheritance(instance, *baseComponent);
	instance["key"] = key;
	instance["enabled"] = true;

	//set up a stringstream to parse the properties

	for (auto prop : props) {
		for (const std::string& prop : props) {
			std::string type, name, value;
			parseTypedKeyValue(prop, type, name, value);

			if (type == "int") {
				instance[name] = std::stoi(value);
			}
			else if (type == "float") {
				instance[name] = std::stof(value);
			}
			else if (type == "bool") {
				instance[name] = (value == "true");
			}
			else {
				instance[name] = value;
			}
		}
	}

	return std::make_shared<luabridge::LuaRef>(instance);
}

void ComponentDB::updateInstance(std::shared_ptr<luabridge::LuaRef> instance, const rapidjson::Value& properties, std::string typeIn)
{
	if (!instance) {
		std::cout << "error: failed to locate component instance";
		exit(0);
	}

	// Handle native C++ components
	if (typeIn == "Rigidbody") {
		if (!instance->isUserdata()) {
			std::cout << "error: Rigidbody component is not userdata" << std::endl;
			return;
		}

		Rigidbody* rb = instance->cast<Rigidbody*>();
		if (!rb) {
			std::cout << "error: failed to cast LuaRef to Rigidbody" << std::endl;
			return;
		}

		for (auto itr = properties.MemberBegin(); itr != properties.MemberEnd(); ++itr) {
			const std::string& name = itr->name.GetString();

			if (name == "x") rb->x = itr->value.GetFloat();
			else if (name == "y") rb->y = itr->value.GetFloat();
			else if (name == "body_type") rb->body_type = itr->value.GetString();
			else if (name == "gravity_scale") rb->gravity_scale = itr->value.GetFloat();
			else if (name == "density") rb->density = itr->value.GetFloat();
			else if (name == "angular_friction") rb->angular_friction = itr->value.GetFloat();
			else if (name == "rotation") rb->rotation = itr->value.GetFloat();
			else if (name == "has_collider") rb->has_collider = itr->value.GetBool();
			else if (name == "has_trigger") rb->has_trigger = itr->value.GetBool();
			else if (name == "collider_type") rb->collider_type = itr->value.GetString();
			else if (name == "width") rb->width = itr->value.GetFloat();
			else if (name == "height") rb->height = itr->value.GetFloat();
			else if (name == "radius") rb->radius = itr->value.GetFloat();
			else if (name == "friction") rb->friction = itr->value.GetFloat();
			else if (name == "bounciness") rb->bounciness = itr->value.GetFloat();
			else if (name == "trigger_height") rb->trigger_height = itr->value.GetFloat();
			else if (name == "trigger_width") rb->trigger_width = itr->value.GetFloat();
			else if (name == "trigger_radius") rb->trigger_radius = itr->value.GetFloat();
			else if (name == "trigger_type") rb->trigger_type = itr->value.GetString();
		}

		return;
	}
	else if (typeIn == "ParticleSystem") {
		if (!instance->isUserdata()) {
			std::cout << "error: Rigidbody component is not userdata" << std::endl;
			return;
		}

		ParticleSystem* ps = instance->cast<ParticleSystem*>();
		if (!ps) {
			std::cout << "error: failed to cast LuaRef to Rigidbody" << std::endl;
			return;
		}

		for (auto itr = properties.MemberBegin(); itr != properties.MemberEnd(); ++itr) {
			const std::string name = itr->name.GetString();
			const auto& value = itr->value;

			if (name == "x") ps->x = value.GetFloat();
			else if (name == "y") ps->y = value.GetFloat();
			else if (name == "frames_between_bursts") ps->frames_between_bursts = std::max(1, value.GetInt());
			else if (name == "burst_quantity") ps->burst_quantity = std::max(1, value.GetInt());
			else if (name == "start_scale_min") ps->start_scale_min = value.GetFloat();
			else if (name == "start_scale_max") ps->start_scale_max = value.GetFloat();
			else if (name == "rotation_min") ps->rotation_min = value.GetFloat();
			else if (name == "rotation_max") ps->rotation_max = value.GetFloat();
			else if (name == "start_color_r") ps->start_color_r = value.GetInt();
			else if (name == "start_color_g") ps->start_color_g = value.GetInt();
			else if (name == "start_color_b") ps->start_color_b = value.GetInt();
			else if (name == "start_color_a") ps->start_color_a = value.GetInt();
			else if (name == "emit_radius_min") ps->emit_radius_min = value.GetFloat();
			else if (name == "emit_radius_max") ps->emit_radius_max = value.GetFloat();
			else if (name == "emit_angle_min") ps->emit_angle_min = value.GetFloat();
			else if (name == "emit_angle_max") ps->emit_angle_max = value.GetFloat();
			else if (name == "image") ps->image = value.GetString();
			else if (name == "sorting_order") ps->sorting_order = value.GetInt();
			else if (name == "duration_frames") ps->duration_frames = std::max(1, value.GetInt());
			else if (name == "start_speed_min") ps->start_speed_min = value.GetFloat();
			else if (name == "start_speed_max") ps->start_speed_max = value.GetFloat();
			else if (name == "rotation_speed_min") ps->rotation_speed_min = value.GetFloat();
			else if (name == "rotation_speed_max") ps->rotation_speed_max = value.GetFloat();
			else if (name == "gravity_scale_x") ps->gravity_scale_x = value.GetFloat();
			else if (name == "gravity_scale_y") ps->gravity_scale_y = value.GetFloat();
			else if (name == "drag_factor") ps->drag_factor = value.GetFloat();
			else if (name == "angular_drag_factor") ps->angular_drag_factor = value.GetFloat();
			else if (name == "end_scale") ps->end_scale.emplace(value.GetFloat());
			else if (name == "end_color_r") ps->end_color_r.emplace(value.GetInt());
			else if (name == "end_color_g") ps->end_color_g.emplace(value.GetInt());
			else if (name == "end_color_b") ps->end_color_b.emplace(value.GetInt());
			else if (name == "end_color_a") ps->end_color_a.emplace(value.GetInt());


		}

		// Clamp and setup random engines
		ps->angleGen = RandomEngine(ps->emit_angle_min, ps->emit_angle_max, 298);
		ps->radiusGen = RandomEngine(ps->emit_radius_min, ps->emit_radius_max, 404);
		ps->rotationGen = RandomEngine(ps->rotation_min, ps->rotation_max, 440);
		ps->scaleGen = RandomEngine(ps->start_scale_min, ps->start_scale_max, 494);
		ps->speedGen = RandomEngine(ps->start_speed_min, ps->start_speed_max, 498);
		ps->rotationSpeedGen = RandomEngine(ps->rotation_speed_min, ps->rotation_speed_max, 305);
		
		return;
	}

	if (!instance->isTable()) {
		std::cout << "error: failed to locate component instance";
		exit(0);
	}

	// Default: treat as Lua table
	for (auto itr = properties.MemberBegin(); itr != properties.MemberEnd(); ++itr) {
		if (itr->value.IsString())
			(*instance)[itr->name.GetString()] = itr->value.GetString();
		else if (itr->value.IsInt())
			(*instance)[itr->name.GetString()] = itr->value.GetInt();
		else if (itr->value.IsFloat())
			(*instance)[itr->name.GetString()] = itr->value.GetFloat();
		else if (itr->value.IsBool())
			(*instance)[itr->name.GetString()] = itr->value.GetBool();
	}
}

void ComponentDB::callFunction(ComponentInfo& compInfo, const std::string& functionName, const std::string& actorName)
{	

	auto& instance = *compInfo.luaRef;

	if (instance.isTable()) {
		if (instance["enabled"].isBool())
			compInfo.enabled = instance["enabled"].cast<bool>();
	}
	if (!compInfo.enabled && functionName != "OnDestroy") return;

	if (functionName == "OnStart") {
		if (compInfo.hasStarted) return;
		compInfo.hasStarted = true;
	}

	try {
		auto functionRef = instance[functionName];
		if (functionRef.isFunction()) {
			functionRef(instance); // Lua method
		}
		
	}
	catch (luabridge::LuaException& e) {
		ReportError(actorName, e);
	}
}

void ComponentDB::establishInheritance(luabridge::LuaRef& instanceTable, luabridge::LuaRef& parentTable)
{
	luabridge::LuaRef metaTable = luabridge::newTable(lua_state);
	metaTable["__index"] = parentTable;
	instanceTable.push(lua_state);
	metaTable.push(lua_state);
	lua_setmetatable(lua_state, -2);
	lua_pop(lua_state, 1);
}


void ComponentDB::debugLog(luabridge::LuaRef message)
{
	if (message.isNil()) {
		std::cout << std::endl;
	}
	else if (message.isString()) {
		std::cout << message.cast<std::string>() << std::endl;
	}
	else if (message.isNumber()) {
		std::cout << message.cast<float>() << std::endl;
	}
	else if (message.isBool()) {
		std::cout << (message.cast<bool>() ? "true" : "false") << std::endl;
	}
}

void ComponentDB::ReportError(const std::string& actorName, const luabridge::LuaException& e)
{
	std::string message = e.what();
	std::replace(message.begin(), message.end(), '\\', '/');
	std::cout << "\033[31m" << actorName << " : " << message << "\033[0m" << std::endl;

}

void ContactListener::BeginContact(b2Contact* contact) {
	b2Fixture* aF = contact->GetFixtureA();
	b2Fixture* bF = contact->GetFixtureB();
	actorDB::Actor* a = reinterpret_cast<actorDB::Actor*>(aF->GetUserData().pointer);
	actorDB::Actor* b = reinterpret_cast<actorDB::Actor*>(bF->GetUserData().pointer);
	if (!a || !b) return;

	bool isTriggerA = aF->IsSensor();
	bool isTriggerB = bF->IsSensor();


	b2WorldManifold wm;
	contact->GetWorldManifold(&wm);

	b2Vec2 point = wm.points[0];
	b2Vec2 normal = wm.normal;
	b2Vec2 relativeVelocity = aF->GetBody()->GetLinearVelocity() - bF->GetBody()->GetLinearVelocity();

	Collision colA{ b, point, normal, relativeVelocity };
	Collision colB{ a, point, -normal, relativeVelocity };

	if (isTriggerA && isTriggerB) {
		// Trigger-trigger -> OnTriggerEnter
		b2Vec2 sentinel(-999.0f, -999.0f);
		Collision colA{ b, sentinel, sentinel, relativeVelocity };
		Collision colB{ a, sentinel, sentinel, relativeVelocity };

		for (const auto& key : a->keys) {
			auto& comp = a->components[key];
			if ((*comp.luaRef)["OnTriggerEnter"].isFunction()) {
				(*comp.luaRef)["OnTriggerEnter"](*comp.luaRef, colA);
			}
		}

		for (const auto& key : b->keys) {
			auto& comp = b->components[key];
			if ((*comp.luaRef)["OnTriggerEnter"].isFunction()) {
				(*comp.luaRef)["OnTriggerEnter"](*comp.luaRef, colB);
			}
		}
	}
	else if (!isTriggerA && !isTriggerB) {
		// Collider-collider -> OnCollisionEnter
		b2WorldManifold wm;
		contact->GetWorldManifold(&wm);
		Collision colA{ b, wm.points[0], wm.normal, relativeVelocity };
		Collision colB{ a, wm.points[0], wm.normal, relativeVelocity };

		for (const auto& key : a->keys) {
			auto& comp = a->components[key];
			if ((*comp.luaRef)["OnCollisionEnter"].isFunction()) {
				(*comp.luaRef)["OnCollisionEnter"](*comp.luaRef, colA);
			}
		}

		for (const auto& key : b->keys) {
			auto& comp = b->components[key];
			if ((*comp.luaRef)["OnCollisionEnter"].isFunction()) {
				(*comp.luaRef)["OnCollisionEnter"](*comp.luaRef, colB);
			}
		}
	}
}

void ContactListener::EndContact(b2Contact* contact) {
	b2Fixture* aF = contact->GetFixtureA();
	b2Fixture* bF = contact->GetFixtureB();
	actorDB::Actor* a = reinterpret_cast<actorDB::Actor*>(aF->GetUserData().pointer);
	actorDB::Actor* b = reinterpret_cast<actorDB::Actor*>(bF->GetUserData().pointer);
	if (!a || !b) return;

	bool isTriggerA = aF->IsSensor();
	bool isTriggerB = bF->IsSensor();


	b2Vec2 invalid(-999.0f, -999.0f);
	b2Vec2 relativeVelocity = aF->GetBody()->GetLinearVelocity() - bF->GetBody()->GetLinearVelocity();

	Collision colA{ b, invalid, invalid, relativeVelocity };
	Collision colB{ a, invalid, invalid, relativeVelocity };

	if (isTriggerA && isTriggerB) {
		Collision colA{ b, invalid, invalid, relativeVelocity };
		Collision colB{ a, invalid, invalid, relativeVelocity };

		for (const auto& key : a->keys) {
			auto& comp = a->components[key];
			if ((*comp.luaRef)["OnTriggerExit"].isFunction()) {
				(*comp.luaRef)["OnTriggerExit"](*comp.luaRef, colA);
			}
		}

		for (const auto& key : b->keys) {
			auto& comp = b->components[key];
			if ((*comp.luaRef)["OnTriggerExit"].isFunction()) {
				(*comp.luaRef)["OnTriggerExit"](*comp.luaRef, colB);
			}
		}
	}
	else if (!isTriggerA && !isTriggerB) {
		Collision colA{ b, invalid, invalid, relativeVelocity };
		Collision colB{ a, invalid, invalid, relativeVelocity };

		for (const auto& key : a->keys) {
			auto& comp = a->components[key];
			if ((*comp.luaRef)["OnCollisionExit"].isFunction()) {
				(*comp.luaRef)["OnCollisionExit"](*comp.luaRef, colA);
			}
		}

		for (const auto& key : b->keys) {
			auto& comp = b->components[key];
			if ((*comp.luaRef)["OnCollisionExit"].isFunction()) {
				(*comp.luaRef)["OnCollisionExit"](*comp.luaRef, colB);
			}
		}
	}
}

void ParticleSystem::Burst()
{
	if (!actor || !enabled) return;
	for (int i = 0; i < burst_quantity; ++i) {
		float angleRad = glm::radians(angleGen.Sample());
		float radius = radiusGen.Sample();
		glm::vec2 offset = { cos(angleRad) * radius, sin(angleRad) * radius };

		glm::vec2 basePos = { x, y };
		if (auto* rb = actor->GetComponent("Rigidbody").cast<Rigidbody*>()) {
			basePos.x += rb->x;
			basePos.y += rb->y;
		}

		glm::vec2 position = basePos + offset;
		glm::vec2 velocity = glm::vec2(cos(angleRad), sin(angleRad)) * speedGen.Sample();
		float initial_scale = scaleGen.Sample();
		float scale = initial_scale;
		float rotation = rotationGen.Sample();
		float angular_velocity = rotationSpeedGen.Sample();

		int index;
		if (!freeList.empty()) {
			index = freeList.front();
			freeList.pop();

			positions[index] = position;
			velocities[index] = velocity;
			initial_scales[index] = initial_scale;
			scales[index] = scale;
			rotations[index] = rotation;
			angular_velocities[index] = angular_velocity;
			birth_frames[index] = local_frame;
			actives[index] = true;
		}
		else {
			positions.push_back(position);
			velocities.push_back(velocity);
			initial_scales.push_back(initial_scale);
			scales.push_back(scale);
			rotations.push_back(rotation);
			angular_velocities.push_back(angular_velocity);
			birth_frames.push_back(local_frame);
			actives.push_back(true);
		}
	}
}

void ParticleSystem::OnStart()
{
	if (image.empty()) {
		ImageDB::CreateDefaultParticleTextureWithName(image);
	}
	else {
		ImageDB::LoadImage(image);
	}

}

void ParticleSystem::OnUpdate() {
	if (!enabled || !actor) return;

	// Emit particles if this is a burst frame
	if (local_frame % frames_between_bursts == 0 && !paused) {
		Burst();
	}

	// Render all particles
	int frame = local_frame;
	for (size_t i = 0; i < positions.size(); ++i) {
		if (!actives[i]) continue;

		float age = frame - birth_frames[i];
		if (age >= duration_frames) {
			actives[i] = false;
			freeList.push(i);
			continue;
		}

		float t = age / float(duration_frames);

		// Velocity + gravity + drag
		velocities[i] += glm::vec2(gravity_scale_x, gravity_scale_y);
		velocities[i] *= drag_factor;
		positions[i] += velocities[i];

		angular_velocities[i] *= angular_drag_factor;
		rotations[i] += angular_velocities[i];

		float current_scale = end_scale.has_value() ? glm::mix(initial_scales[i], end_scale.value(), t) : initial_scales[i];

		// Color
		glm::ivec4 color = { start_color_r, start_color_g, start_color_b ,start_color_a};
		if (end_color_r.has_value()) color.r = glm::mix(start_color_r, end_color_r.value(), t);
		if (end_color_g.has_value()) color.g = glm::mix(start_color_g, end_color_g.value(), t);
		if (end_color_b.has_value()) color.b = glm::mix(start_color_b, end_color_b.value(), t);
		if (end_color_a.has_value()) color.a = glm::mix(start_color_a, end_color_a.value(), t);

		ImageDB::Lua_DrawEx(
			image,
			positions[i].x, positions[i].y,
			rotations[i],
			current_scale, current_scale,
			0.5f, 0.5f,
			color.r, color.g, color.b, color.a,
			sorting_order
		);
	}
	// Advance internal frame counter
	local_frame++;

}

void ParticleSystem::OnDestroy()
{
	positions.clear();
	velocities.clear();
	scales.clear();
	initial_scales.clear();
	rotations.clear();
	angular_velocities.clear();
	birth_frames.clear();
	actives.clear();
	while (!freeList.empty()) freeList.pop();
}

