#pragma once

#include <nlohmann/json.hpp>

class OSGB_Encoder
{
public:
	
	bool encode(QString jsonPath, QString encodeKey)
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
		QByteArray xorKey = QCryptographicHash::hash(encodeKey.toLatin1(), QCryptographicHash::Md5).toHex();
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
};

