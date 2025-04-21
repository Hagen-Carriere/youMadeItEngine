#include "TemplateDB.h"

rapidjson::Document TemplateDB::getTemplate(const std::string& templateName)
{
	//load file resources/templates/templateName
	std::string templatePath = "resources/actor_templates/" + templateName + ".template";
	if (!std::filesystem::exists(templatePath)) {
		std::cout << "error: template " + templateName + " is missing";
		std::exit(0);
	}
	std::ifstream file(templatePath);
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string content = buffer.str();

	rapidjson::Document templateDoc;
	templateDoc.Parse(content.c_str());
	return templateDoc;
}
