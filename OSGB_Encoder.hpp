#pragma once

#include <nlohmann/json.hpp>

class OSGB_Encoder
{
public:
	bool encode(QString jsonPath, QByteArray xorKey)
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


		// 加密
		auto encodeString = [&xorKey](QByteArray src)->QByteArray
		{
			for (int i = 0; i < src.size(); i++)
			{
				src[i] = src[i] ^ xorKey[i % xorKey.size()];
			}
			return src.toBase64();
		};

		std::function<void(nlohmann::json& node)> handleBoundingVolume = [&handleBoundingVolume, &encodeString](nlohmann::json& node) {
			if (node.contains("boundingVolume"))
			{
				auto items = node["boundingVolume"].items();
				
				nlohmann::json extraJ;
				for (auto [key, value] : items)
				{
					extraJ[key + "_encrypted"] = encodeString(QByteArray::fromStdString(value.dump())).toStdString();
					for (auto& val : value)
						val = 0;
				}
				node["boundingVolume"].merge_patch(extraJ);
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
	

	bool decode(QString jsonPath, QByteArray xorKey)
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

		// 解密
		auto decodeString = [&xorKey](QByteArray src)->QByteArray
		{
			auto dst = QByteArray::fromBase64(src);
			for (int i = 0; i < dst.size(); i++)
			{
				dst[i] = dst[i] ^ xorKey[i % xorKey.size()];
			}
			return dst;
		};

		std::function<void(nlohmann::json& node)> handleBoundingVolume = [&handleBoundingVolume, &decodeString](nlohmann::json& node) {
			if (node.contains("boundingVolume"))
			{
				auto items = node["boundingVolume"].items();

				nlohmann::json extraJ;
				nlohmann::json removeKeyList;
				for (auto [key, value] : items)
				{
					if (QString::fromStdString(key).endsWith("_encrypted"))
					{
						removeKeyList.push_back(key);
						continue;
					}
					extraJ[key] = nlohmann::json::parse(decodeString(QByteArray::fromStdString(node["boundingVolume"][key + "_encrypted"])).toStdString());
				}
				node["boundingVolume"].merge_patch(extraJ);
				for (const std::string& key : removeKeyList)
				{
					node["boundingVolume"].erase(key);
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

