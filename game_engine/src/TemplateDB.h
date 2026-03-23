#ifndef TEMPLATEDB_H
#define TEMPLATEDB_H


#include "rapidjson/document.h"
#include <string>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <fstream>

class TemplateDB
{
public:
    rapidjson::Document getTemplate(const std::string& templateName);
};

#endif