#pragma once

#include <nlohmann/json.hpp>

class OSGB_Encoder
{
public:
	bool encode(QString jsonPath, QList<double> diffKey)
	{
		// 读取json文件
		QFile jsonFile(jsonPath);
		if (!jsonFile.open(QIODevice::ReadOnly))
		{
			return false;
		}

		auto j{ nlohmann::json::parse(jsonFile.readAll()) };
		jsonFile.close();

		if (j["asset"].contains("encrypted") && j["asset"]["encrypted"])
		{
			return false;
		}
		
		j["asset"]["encrypted"] = true;

		int keyIndex = 0;

		std::function<void(nlohmann::json& node)> handleBoundingVolume = [&handleBoundingVolume, &keyIndex, &diffKey](nlohmann::json& node) {
			if (node.contains("boundingVolume"))
			{
				auto items = node["boundingVolume"].items();

				for (auto [key, value] : items)
				{
					for (auto& val : value)
					{
						val = val.get<double>() + diffKey[keyIndex];
						keyIndex = (keyIndex + 1) % diffKey.size();
					}
				}
			}
			if (node.contains("children"))
			{
				for (auto& child : node["children"])
				{
					handleBoundingVolume(child);
				}
			}
		};

		handleBoundingVolume(j["root"]);

		// 写入json到jsonFile
		if (!jsonFile.open(QIODevice::WriteOnly))
		{
			return false;
		}
		jsonFile.write(j.dump().c_str());
		jsonFile.close();
		return true;
	}
	

	bool decode(QString jsonPath, QList<double> diffKey)
	{
		// 读取json文件
		QFile jsonFile(jsonPath);
		if (!jsonFile.open(QIODevice::ReadOnly))
		{
			return false;
		}

		auto j{ nlohmann::json::parse(jsonFile.readAll()) };
		jsonFile.close();

		if (!j["asset"].contains("encrypted") || !j["asset"]["encrypted"])
		{
			return false;
		}

		j["asset"].erase("encrypted");

		int keyIndex = 0;

		std::function<void(nlohmann::json& node)> handleBoundingVolume = [&handleBoundingVolume, &keyIndex, &diffKey](nlohmann::json& node) {
			if (node.contains("boundingVolume"))
			{
				auto items = node["boundingVolume"].items();

				for (auto [key, value] : items)
				{
					for (auto& val : value)
					{
						val = val.get<double>() - diffKey[keyIndex];
						keyIndex = (keyIndex + 1) % diffKey.size();
					}
				}
			}
			if (node.contains("children"))
			{
				for (auto& child : node["children"])
				{
					handleBoundingVolume(child);
				}
			}
		};

		handleBoundingVolume(j["root"]);

		// 写入json到jsonFile
		if (!jsonFile.open(QIODevice::WriteOnly))
		{
			return false;
		}
		jsonFile.write(j.dump().c_str());
		jsonFile.close();
		return true;
	}
};

