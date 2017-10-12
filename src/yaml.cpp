#include "yaml.hpp"
#include <yaml-cpp/yaml.h>

/// Convert a coordinate system to YAML.
YAML:: Node YAML::convert<dr::yaskawa::CartesianPosition>::encode(dr::yaskawa::CartesianPosition const & in) {
	YAML::Node node;
	node["x"] = in.x();
	node["y"] = in.y();
	node["z"] = in.z();
	node["rx"] = in.rx();
	node["ry"] = in.ry();
	node["rz"] = in.rz();
	node["configuration"] = std::uint8_t(in.configuration());
	node["tool"] = in.tool();
	node["frame"] = in.frame();
	return node;
}

bool YAML::convert<dr::yaskawa::CartesianPosition>::decode(Node const & node, dr::yaskawa::CartesianPosition & out) {
	if (node.IsMap() || (node.size() != 9)) return false;

	out = {
		node["x"].as<double>(),
		node["y"].as<double>(),
		node["z"].as<double>(),
		node["rx"].as<double>(),
		node["ry"].as<double>(),
		node["rz"].as<double>(),
		node["frame"].as<dr::yaskawa::CoordinateSystem>(),
		node["configuration"].as<std::uint8_t>(),
		node["tool"].as<int>()
	};
	return true;
}

YAML::Node YAML::convert<dr::yaskawa::CoordinateSystem>::encode(dr::yaskawa::CoordinateSystem const & in) {
	YAML::Node node;
	node["name"] = dr::yaskawa::toString(in);
	return node;
}

bool YAML::convert<dr::yaskawa::CoordinateSystem>::decode(Node const & node, dr::yaskawa::CoordinateSystem & out) {
	out = dr::yaskawa::toCoordinateSystem(node["name"].as<std::string>()).value();
	return true;
}
