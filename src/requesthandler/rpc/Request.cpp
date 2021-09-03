#include "Request.h"
#include "../../plugin-macros.generated.h"

json GetDefaultJsonObject(json requestData)
{
	// Always provide an object to prevent exceptions while running checks in requests
	if (!requestData.is_object())
		return json::object();
	else
		return requestData;
}

Request::Request(SessionPtr session, std::string requestType, json requestData) :
	Session(session),
	RpcVersion(session->RpcVersion()),
	IgnoreNonFatalRequestChecks(session->IgnoreNonFatalRequestChecks()),
	RequestType(requestType),
	RequestData(GetDefaultJsonObject(requestData))
{
}

const bool Request::ValidateBasic(const std::string keyName, RequestStatus::RequestStatus &statusCode, std::string &comment) const
{
	if (!HasRequestData()) {
		statusCode = RequestStatus::MissingRequestData;
		comment = "Your request data is missing or invalid (non-object)";
		return false;
	}

	if (!RequestData.contains(keyName) || RequestData[keyName].is_null()) {
		statusCode = RequestStatus::MissingRequestParameter;
		comment = std::string("Your request is missing the `") + keyName + "` parameter.";
		return false;
	}

	return true;
}

const bool Request::ValidateNumber(const std::string keyName, RequestStatus::RequestStatus &statusCode, std::string &comment, const double minValue, const double maxValue) const
{
	if (!ValidateBasic(keyName, statusCode, comment))
		return false;

	if (!RequestData[keyName].is_number()) {
		statusCode = RequestStatus::InvalidRequestParameterType;
		comment = std::string("The parameter `") + keyName + "` must be a number.";
		return false;
	}

	double value = RequestData[keyName];
	if (value < minValue) {
		statusCode = RequestStatus::RequestParameterOutOfRange;
		comment = std::string("The parameter `") + keyName + "` is below the minimum of `" + std::to_string(minValue) + "`";
		return false;
	}
	if (value > maxValue) {
		statusCode = RequestStatus::RequestParameterOutOfRange;
		comment = std::string("The parameter `") + keyName + "` is above the maximum of `" + std::to_string(maxValue) + "`";
		return false;
	}

	return true;
}

const bool Request::ValidateString(const std::string keyName, RequestStatus::RequestStatus &statusCode, std::string &comment, const bool allowEmpty) const
{
	if (!ValidateBasic(keyName, statusCode, comment))
		return false;

	if (!RequestData[keyName].is_string()) {
		statusCode = RequestStatus::InvalidRequestParameterType;
		comment = std::string("The parameter `") + keyName + "` must be a string.";
		return false;
	}

	if (RequestData[keyName].get<std::string>().empty() && !allowEmpty) {
		statusCode = RequestStatus::RequestParameterEmpty;
		comment = std::string("The parameter `") + keyName + "` must not be empty.";
		return false;
	}

	return true;
}

const bool Request::ValidateBoolean(const std::string keyName, RequestStatus::RequestStatus &statusCode, std::string &comment) const
{
	if (!ValidateBasic(keyName, statusCode, comment))
		return false;

	if (!RequestData[keyName].is_boolean()) {
		statusCode = RequestStatus::InvalidRequestParameterType;
		comment = std::string("The parameter `") + keyName + "` must be boolean.";
		return false;
	}

	return true;
}

const bool Request::ValidateObject(const std::string keyName, RequestStatus::RequestStatus &statusCode, std::string &comment, const bool allowEmpty) const
{
	if (!ValidateBasic(keyName, statusCode, comment))
		return false;

	if (!RequestData[keyName].is_object()) {
		statusCode = RequestStatus::InvalidRequestParameterType;
		comment = std::string("The parameter `") + keyName + "` must be an object.";
		return false;
	}

	if (RequestData[keyName].empty() && !allowEmpty) {
		statusCode = RequestStatus::RequestParameterEmpty;
		comment = std::string("The parameter `") + keyName + "` must not be empty.";
		return false;
	}

	return true;
}

const bool Request::ValidateArray(const std::string keyName, RequestStatus::RequestStatus &statusCode, std::string &comment, const bool allowEmpty) const
{
	if (!ValidateBasic(keyName, statusCode, comment))
		return false;

	if (!RequestData[keyName].is_array()) {
		statusCode = RequestStatus::InvalidRequestParameterType;
		comment = std::string("The parameter `") + keyName + "` must be an array.";
		return false;
	}

	if (RequestData[keyName].empty() && !allowEmpty) {
		statusCode = RequestStatus::RequestParameterEmpty;
		comment = std::string("The parameter `") + keyName + "` must not be empty.";
		return false;
	}

	return true;
}

obs_source_t *Request::ValidateSource(const std::string keyName, RequestStatus::RequestStatus &statusCode, std::string &comment) const
{
	if (!ValidateString(keyName, statusCode, comment))
		return nullptr;

	std::string sourceName = RequestData[keyName];

	obs_source_t *ret = obs_get_source_by_name(sourceName.c_str());
	if (!ret) {
		statusCode = RequestStatus::ResourceNotFound;
		comment = std::string("No source was found by the name of `") + sourceName + "`.";
		return nullptr;
	}

	return ret;
}

obs_source_t *Request::ValidateScene(const std::string keyName, RequestStatus::RequestStatus &statusCode, std::string &comment, const ObsWebSocketSceneFilter filter) const
{
	obs_source_t *ret = ValidateSource(keyName, statusCode, comment);
	if (!ret)
		return nullptr;

	if (obs_source_get_type(ret) != OBS_SOURCE_TYPE_SCENE) {
		obs_source_release(ret);
		statusCode = RequestStatus::InvalidResourceType;
		comment = "The specified source is not a scene.";
		return nullptr;
	}

	bool isGroup = obs_source_is_group(ret);
	if (filter == OBS_WEBSOCKET_SCENE_FILTER_SCENE_ONLY && isGroup) {
		obs_source_release(ret);
		statusCode = RequestStatus::InvalidResourceType;
		comment = "The specified source is not a scene.";
		return nullptr;
	} else if (filter == OBS_WEBSOCKET_SCENE_FILTER_GROUP_ONLY && !isGroup) {
		obs_source_release(ret);
		statusCode = RequestStatus::InvalidResourceType;
		comment = "The specified source is not a group.";
		return nullptr;
	}

	return ret;
}

obs_source_t *Request::ValidateInput(const std::string keyName, RequestStatus::RequestStatus &statusCode, std::string &comment) const
{
	obs_source_t *ret = ValidateSource(keyName, statusCode, comment);
	if (!ret)
		return nullptr;

	if (obs_source_get_type(ret) != OBS_SOURCE_TYPE_INPUT) {
		obs_source_release(ret);
		statusCode = RequestStatus::InvalidResourceType;
		comment = "The specified source is not an input.";
		return nullptr;
	}

	return ret;
}

obs_sceneitem_t *Request::ValidateSceneItem(const std::string sceneKeyName, const std::string sceneItemIdKeyName, RequestStatus::RequestStatus &statusCode, std::string &comment, const ObsWebSocketSceneFilter filter) const
{
	OBSSource sceneSource = ValidateScene(sceneKeyName, statusCode, comment, filter);
	obs_source_release(sceneSource);
	if (!sceneSource)
		return nullptr;

	if (!ValidateNumber(sceneItemIdKeyName, statusCode, comment, 0))
		return nullptr;

	OBSScene scene = obs_scene_from_source(sceneSource);

	int64_t sceneItemId = RequestData[sceneItemIdKeyName];

	OBSSceneItem sceneItem = obs_scene_find_sceneitem_by_id(scene, sceneItemId);
	if (!sceneItem) {
		statusCode = RequestStatus::ResourceNotFound;
		comment = std::string("No scene items were found in scene `") + RequestData[sceneKeyName].get<std::string>() + "` with the ID `" + std::to_string(sceneItemId) + "`.";
		return nullptr;
	}

	obs_sceneitem_addref(sceneItem);
	return sceneItem;
}
