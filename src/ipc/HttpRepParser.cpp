
#include "HttpRepParser.h"
#include "global.h"

#include "hv/hlog.h"
#include <stdexcept>
#include <thread>

//----------------------------HttpClientWrapper----------------------------------------------
//----------------------------HttpClientWrapper----------------------------------------------
//----------------------------HttpClientWrapper----------------------------------------------

//http_login_param
SerConnParam::SerConnParam()
{
	Clear();
}

bool SerConnParam::IsEmpty()
{
	if (ip.isEmpty() || port.isEmpty() || userName.isEmpty() || pwd.isEmpty())
	{
		return true;
	}
	return false;
}

void SerConnParam::Clear()
{
	ip.clear();
	port.clear();
	userName.clear();
	pwd.clear();
}


DevBindparam::DevBindparam()
{
	Clear();
}

bool DevBindparam::IsEmpty()
{
	if (devId.empty() || devType == 0)
	{
		return true;
	}
	return false;
}

void DevBindparam::Clear()
{
	devId.clear();
	devType = 0;
	devStatus = DeviceStatus::IDLE;
}

//DevBindparam DevBindparam::fromJson(const std::string& json_str)
//{
//
//}

void DevBindparam::FromJson(const nlohmann::json& jsonArr)
{
	for (const auto& item : jsonArr)
	{
		if (item.is_object())
		{
			DeviceInfo info;
			nlohmann::json obj = item;
			info.devId = obj["device_id"].get<std::string>();
			info.devType = obj["device_type"].get<int>();
			auto devStatus = obj["status"].get<int>();
			info.devStatus = static_cast<DeviceStatus>(devStatus);
			devVec.emplace_back(info);
		}
	}

	////// 验证必要字段存在
	////if (!j.contains("deviceId") || !j.contains("deviceType") || !j.contains("status")) 
	////{
	////	//throw抛出的使用
	////	throw std::invalid_argument("Missing required fields in DeviceInfo JSON");
	////}

	////// 验证字段类型和内容
	////if (!j["deviceId"].is_string() || j["deviceId"].get<std::string>().empty()) 
	////{
	////	throw std::invalid_argument("Invalid deviceId field");
	////}
	////if (!j["deviceType"].is_number_integer()) 
	////{
	////	throw std::invalid_argument("Invalid deviceType field");
	////}
	////if (!j["status"].is_number_integer()) 
	////{
	////	throw std::invalid_argument("Invalid status field");
	////}
	//info.devId = j["device_id"].get<std::string>();
	//info.devType = j["status"].get<int>();
	//int status_int = j["status"].get<int>();
	//info.devStatus = static_cast<DeviceStatus>(status_int);

	////// 验证状态值范围
	////if (status_int < 1 || status_int > 5)
	////{
	////	throw std::invalid_argument("Invalid status value");
	////}
	//info.devStatus = static_cast<DeviceStatus>(status_int);
}


//order_info_param
UserInfoParam::UserInfoParam()
{
	Clear();
}

bool UserInfoParam::IsEmpty()
{
	if (userId.empty() || userStaffName.empty() || userPhoneNum.empty() || usereEmail.empty() ||
		userDepartment == 0 || userCity == 0 || userStatus == 0)
	{
		return true;
	}
	return false;
}

void UserInfoParam::Clear()
{
	userId.clear();
	userStaffName.clear();
	userPhoneNum.clear();
	usereEmail.clear();
	userDepartment = 0;
	userCity = 0;
	userStatus = 0;
}

void UserInfoParam::FromJson(const nlohmann::json& jsonObj)
{
	userId = jsonObj["id"].get<std::string>();
	userStaffName = jsonObj["staff_name"].get<std::string>();
	userPhoneNum = jsonObj["phone_number"].get<std::string>();
	usereEmail = jsonObj["email"].get<std::string>();
	userDepartment = jsonObj["department"].get<int>();
	userCity = jsonObj["city"].get<int>();
	userStatus = jsonObj["status"].get<int>();
}

//oss_param
OSSTokenParam::OSSTokenParam()
{
	Clear();
}

bool OSSTokenParam::IsEmpty()
{
	if (ossId.empty() || ossSerct.empty() || ossToken.empty() ||
		ossBucket.empty() || ossEndPoint.empty() || ossRegin.empty() || ossExpireTime.empty())
	{
		return true;
	}
	return false;
}

void OSSTokenParam::Clear()
{
	ossId.clear();
	ossSerct.clear();
	ossToken.clear();
	ossBucket.clear();
	ossEndPoint.clear();
	ossRegin.clear();
	ossExpireTime.clear();
}

void OSSTokenParam::FromJson(const nlohmann::json& jsonObj)
{
	ossId		= jsonObj["access_key_id"].get<std::string>();
	ossSerct	= jsonObj["access_key_secret"].get<std::string>();
	ossToken	= jsonObj["security_token"].get<std::string>();
	ossBucket	= jsonObj["bucket"].get<std::string>();
	ossEndPoint = jsonObj["endpoint"].get<std::string>();
	ossRegin	= jsonObj["region"].get<std::string>();
	ossExpireTime = jsonObj["expire_time"].get<long long>();
}

//ser_order_param
SerOrderInfoParam::SerOrderInfoParam()
{
	Clear();
}

bool SerOrderInfoParam::IsEmpty()
{
	//todo
	return false;
}

void SerOrderInfoParam::Clear()
{
	sopId.clear();
	sopType = 0;
	sopProId = 0;
	sopSerId.clear();
	sopStoreId = 0;
	sopSkuId = 0;
	sopSerialNum.clear();
	sopName.clear();
	sopCostTtime = 0;
	sopRemarks.clear();
	sopSerStep = 0;
}

void SerOrderInfoParam::FromJson(const nlohmann::json& json)
{
	sopId = json["order_id"].get<std::string>();
	sopType = json["order_type"].get<int>();
	sopProId = json["product_id"].get<int>();
	sopSerId = json["service_id"].get<std::string>();
	sopStoreId = json["store_id"].get<long long>();
	sopSkuId = json["sku_id"].get<long long>();
	sopSerialNum = json["serial_number"].get<std::string>();
	sopName = json["name"].get<std::string>();
	sopStatus = json["status"].get<int>();
	sopCreateTime = static_cast<time_t>(json["create_time"].get<long long>());
	sopOrderTime = static_cast<time_t>(json["order_time"].get<long long>());
	sopCostTtime = json["cost_time"].get<long long>();
	sopRemarks = json["remarks"].get<std::string>();
	sopSerStep = json["step"].get<int>();
}


SerProParam::SerProParam()
{
	sppSerId = "";
	sppProgressNum = 0;
	sppFileList.clear();
	sppType = 0;
}

void SerProParam::FromJson(const nlohmann::json& json)
{
	//TODO: 判断当前数据是否为一个arr
	auto jsonArr = nlohmann::json::array();
	for (const auto& item : json)
	{
		if (item.is_object())
		{
			nlohmann::json obj;
			int finger_id = obj["finger_id"].get<int>();
			std::string file_path = obj["cnc_file_path"].get<std::string>();
			sppFileList.emplace_back(finger_id, file_path);
		}
	}
}


std::string SerProParam::CreateJsonReq()
{
	// 上传文件
	QJsonObject json;
	json["service_id"] = QString::fromStdString(sppSerId);
	if (sppType == 1)
	{
		QJsonArray modelArray;
		for (auto&[fingerId, modelPath] : sppFileList)
		{
			QJsonObject item;
			item["finger_id"] = fingerId;
			item["model_path"] = QString::fromStdString(modelPath);
			modelArray.append(item);
		}
		json["model_list"] = modelArray;
	}
	// 状态改变
	else if (sppType == 2)
	{
		json["progress"] = sppProgressNum;
	}
	return QJsonDocument(json).toJson(QJsonDocument::Indented).toStdString();
}

ErrInfo::ErrInfo()
	: err_code(-1)
	, err_msg("")
	, bValid(false)
{

}


void ErrInfo::ParseFormatMsg(const std::string& msg)
{
	//"err_code=数字, err_msg=文本" 格式
	std::regex pattern(R"(err_code=(\d+),\s*err_msg=([^,]+))");
	std::smatch match;

	if (std::regex_search(msg, match, pattern) && match.size() >= 3)
	{
		err_code = std::stoi(match[1]);
		err_msg = match[2];
		bValid = true;
	}
	else
	{
		// 未匹配到特定格式，使用原始消息
		err_msg = msg;
		bValid = false;
	}
}



//http_base_rep
BaseSerResp::BaseSerResp(int code /*= -1*/, const std::string& msg/*=""*/)
	: status_code(code)
	, status_msg(msg)
{
	err.ParseFormatMsg(msg);
}


void BaseSerResp::FromJosn(const nlohmann::json& json)
{
	status_code = json["status_code"].get<int>();
	auto tmpStr = QString::fromStdString(json["status_msg"].get<std::string>());
	status_msg = tmpStr.toLocal8Bit().toStdString();
	err.ParseFormatMsg(status_msg);
}

bool BaseSerResp::IsSuccess() const
{
	return status_code == 0;
}

std::string BaseSerResp::GetMsg() const
{
	return status_msg;
}

ErrInfo BaseSerResp::GetErr() const
{
	return err;
}

std::unique_ptr<BaseSerResp> BaseSerResp::CreateResponse(const std::string& jsonData)
{
	if (jsonData.empty())
	{
		throw std::invalid_argument("Json string is empty");
		return std::make_unique<BaseSerResp>(-1, "Invalid JSON format");
	}

	nlohmann::json json = nlohmann::json::parse(jsonData);

	// 根据JSON内容判断创建哪种类型的响应对象
	// http_conn_login
	if (json.contains("staff_info"))
	{
		auto response = std::make_unique<UserResp>();
		response->FromJosn(json);
		return response;
	}
	if (json.contains("user_info"))
	{
		auto response = std::make_unique<UserResp>();
		response->FromJosn(json);
		return response;
	}
	//判断是否为oss_token
	else if (json.contains("token"))
	{
		auto response = std::make_unique<OssTokenResp>();
		response->FromJosn(json);
		return response;
	}
	else if (json.contains("service_info") )
	{
		auto response = std::make_unique<SerOrderResp>();
		response->FromJosn(json);
		return response;
	}
	else if (json.contains("plan_list"))
	{
		auto response = std::make_unique<AlgDisposeResp>();
		response->FromJosn(json);
		return response;
	}
	else if (json.contains("device_infos"))
	{
		auto response = std::make_unique<DevBindResp>();
		response->FromJosn(json);
		return response;
	}
	else if (json.contains("device_service_map"))
	{
		auto response = std::make_unique<DevSereMapResp>();
		response->FromJosn(json);
		return response;
	}
	else
	{
		auto response = std::make_unique<ErrResp>();
		response->FromJosn(json);
		return response;
	}
	// 默认返回基类
	return std::make_unique<BaseSerResp>();

	//if (json_str.empty())
	//{
	//	throw std::invalid_argument("Json string is empty");
	//}

	//nlohmann::json j = nlohmann::json::parse(json_str);
	//WSMsgBase msg;

	////// 验证必要字段存在
	////if (!j.contains("msgId") || !j.contains("msgType") || !j.contains("ts") || !j.contains("type")) {
	////	throw std::invalid_argument("Missing required fields in WSMsgBase JSON");
	////}


	//msg.msgType = j["msg_type"].get<std::string>();
	////parse: string -> enum
	////parse payload_data
	//msg.type = onvertStr2Enum(msg.msgType);
	////msg.type = static_cast<MessageType>(typeInt);
	//msg.msgId = j["msg_id"].get<std::string>();
	//msg.ts = j["ts"].get<long long>();
	////TODO: 新增判断

	//msg.payload = j.value("payload", nlohmann::json{});
	//return msg;
}






std::string BaseSerResp::GetString(const nlohmann::json& json, const std::string& key, const std::string& val /*= ""*/)
{
	if (!json.contains(key) || json[key].is_null()) 
	{
		return val;
	}

	try {
		if (json[key].is_string()) 
		{
			std::string result = json[key].get<std::string>();
			// libhv JSON库默认处理UTF-8编码的字符串
			// 中文字符应该能够正确解析和存储
			return result;
		}
		else 
		{
			// 如果不是字符串类型，尝试转换为字符串
			return json[key].dump();
		}
	}
	catch (const std::exception& e) 
	{
		// 解析失败返回默认值
		return val;
	}
}

void OssTokenResp::FromJosn(const nlohmann::json& json)
{
	BaseSerResp::FromJosn(json);
	if (json.contains("token"))
	{
		ossParam.FromJson(json["token"]);
	}
}


void UserResp::FromJosn(const nlohmann::json& json)
{
	BaseSerResp::FromJosn(json);
	//token = json["token"].get<std::string>();
	token = json["token"].get<std::string>();


	//BaseSerResp::FromJosn(json);
	//token = json["token"].get<std::string>();
	//if (json.contains("user_info") && json["user_info"].isObject())
	//{
	//	userParam.FromJson(json["user_info"].toObject());
	//}
}



ErrResp::ErrResp(int code, const std::string& msg)
	: SBaseSerResp(code, msg)
{

}

void ErrResp::FromJosn(const nlohmann::json& json)
{
	SBaseSerResp::FromJosn(json);
	err.err_code = status_code;
	err.err_msg = status_msg;
	err.bValid = true;
}


void SerOrderResp::FromJosn(const nlohmann::json& json)
{
	BaseSerResp::FromJosn(json);
	if (json.contains("service_info"))
	{
		serOrderParam.FromJson(json["service_info"]);
	}
}

void AlgDisposeResp::FromJosn(const nlohmann::json& json)
{
	SBaseSerResp::FromJosn(json);
	if (json.contains("plan_list") && json["plan_list"].is_array())
	{
		serData.FromJson(json["plan_list"]);

	}
}

void DevBindResp::FromJosn(const nlohmann::json& json)
{
	SBaseSerResp::FromJosn(json);
	if (json.contains("device_infos") && json["device_infos"].is_array())
	{
	//	for (const auto& item : json)
	//	{
	//		if (item.isObject())
	//		{
	//			QJsonObject obj = item.toObject();
	//			int finger_id = obj["finger_id"].toInt();
	//			std::string file_path = obj["cnc_file_path"].get<std::string>();
	//			sppFileList.emplace_back(finger_id, file_path);
	//		}
	//	}
		devData.FromJson(json["device_infos"]);
	}
}




SkuParam::SkuParam()
{
	Clear();
}

void SkuParam::Clear()
{
	skuId = 0;
	skuType = 0;
	productId = 0;
	status = 0;
	count = 0;
	price = 0;
	name.clear();
	img.clear();
	costTime = 0;
	paramId.clear();
	//countList.clear();
}

void SkuParam::FromJson(const nlohmann::json& json)
{
	skuId = json.contains("sku_id") ? json["sku_id"].get<int64_t>() : 0;
	skuType = json.contains("sku_type") ? json["sku_type"].get<int>() : 0;
	productId = json.contains("product_id") ? json["product_id"].get<int>() : 0;
	status = json.contains("status") ? json["status"].get<int>() : 0;
	count = json.contains("count") ? json["count"].get<int>() : 0;
	price = json.contains("price") ? json["price"].get<int>() : 0;
	//todo: 安全获取对应字段
	//name = BaseSerResp::SafeGetString(json, "name", "");
	//img = BaseSerResp::SafeGetString(json, "image", "");
	name = json.contains("name") ? json["name"].get<std::string>() : "";
	img = json.contains("image") ? json["image"].get<std::string>() : "";
	costTime = json.contains("cost_time") ? json["cost_time"].get<int>() : 0;

	// 解析param_ids数组
	if (json.contains("param_ids") && json["param_ids"].is_array()) 
	{
		paramId.clear();
		for (const auto& param_id : json["param_ids"]) 
		{
			if (param_id.is_number()) {
				paramId.push_back(param_id.get<int>());
			}
		}
	}

	//// 解析count_list数组
	//if (json.contains("count_list") && json["count_list"].is_array()) 
	//{
	//	countList.clear();
	//	for (const auto& count_item : json["count_list"]) 
	//	{
	//		if (count_item.is_object()) 
	//		{
	//			CountListItem item;
	//			item.type = count_item.contains("type") ? count_item["type"].get<int>() : 0;
	//			item.countName = count_item.contains("image") ? json["image"].get<std::string>() : "";
	//			item.order = count_item.contains("order") ? count_item["order"].get<int>() : 0;

	//			// 解析param_list数组
	//			if (count_item.contains("param_list") && count_item["param_list"].is_array()) 
	//			{
	//				for (const auto& param_item : count_item["param_list"]) 
	//				{
	//					if (param_item.is_object()) 
	//					{
	//						CountListItem::ParamIt param;
	//						param.id = param_item.contains("id") ? param_item["id"].get<int>() : 0;
	//						img = json.contains("image") ? json["image"].get<std::string>() : "";
	//						param.paramName = count_item.contains("name") ? json["name"].get<std::string>() : "";
	//						param.order = param_item.contains("order") ? param_item["order"].get<int>() : 0;
	//						item.paramList.push_back(param);
	//					}
	//				}
	//			}
	//			countList.push_back(item);
	//		}
	//	}
	//}
}

SerProcess::SerProcess()
{
	Clear();
}

void SerProcess::Clear()
{
	serProId.clear();
	proTemplateId.clear();
	name.clear();
	icon.clear();
	type = 0;
	selectType = 0;
	status = 0;
	resCode = 0;
	resMsg.clear();
	proStarTime = 0;
	proEndTime = 0;
	startTime = 0;
	endTime = 0;
	devId.clear();
	serStaff.clear();
}

void SerProcess::FromJson(const nlohmann::json& json)
{
	//serProId = BaseSerRes::SafeGetString(json, "service_process_id", "");
	//process_template_id = BaseSerRes::SafeGetString(json, "process_template_id", "");
	//name = BaseSerRes::SafeGetString(json, "name", "");
	//icon = BaseSerRes::SafeGetString(json, "icon", "");
	//result_msg = BaseSerRes::SafeGetString(json, "result_msg", "");
	//device_id = BaseSerRes::SafeGetString(json, "device_id", "");
	//serve_staff = BaseSerRes::SafeGetString(json, "serve_staff", "");

	serProId = json.contains("service_process_id") ? json["service_process_id"].get<std::string>() : "";
	proTemplateId = json.contains("process_template_id") ? json["process_template_id"].get<std::string>() : "";
	name = json.contains("name") ? json["name"].get<std::string>() : "";
	icon = json.contains("icon") ? json["icon"].get<std::string>() : "";
	resMsg = json.contains("result_msg") ? json["result_msg"].get<std::string>() : "";
	devId = json.contains("device_id") ? json["device_id"].get<std::string>() : "";
	serStaff = json.contains("serve_staff") ? json["serve_staff"].get<std::string>() : "";

	type = json.contains("type") ? json["type"].get<int>() : 0;
	selectType = json.contains("select_type") ? json["select_type"].get<int>() : 0;
	status = json.contains("status") ? json["status"].get<int>() : 0;
	resCode = json.contains("result_code") ? json["result_code"].get<int>() : 0;
	proStarTime = json.contains("process_start_time") ? json["process_start_time"].get<int64_t>() : 0;
	proEndTime = json.contains("process_end_time") ? json["process_end_time"].get<int64_t>() : 0;
	startTime = json.contains("start_time") ? json["start_time"].get<int64_t>() : 0;
	endTime = json.contains("end_time") ? json["end_time"].get<int64_t>() : 0;

}

SerInfo::SerInfo()
{
	Clear();
}

void SerInfo::Clear()
{
	serId.clear();
	serialName.clear();
	storeId = 0;
	orderId.clear();
	userId.clear();
	templateId.clear();
	status = 0;
	planStarTime = 0;
	planEndTime = 0;
	startTime = 0;
	endTime = 0;
	proType = 0;
	proVec.clear();
	remarks.clear();
}

void SerInfo::FromJson(const nlohmann::json& json)
{
	serId = BaseSerResp::GetString(json, "service_id", "");
	serialName = BaseSerResp::GetString(json, "serial_number", "");
	storeId = json.contains("store_id") ? json["store_id"].get<int>() : 0;
	orderId = BaseSerResp::GetString(json, "order_id", "");
	userId = json.contains("user_id") ? json["user_id"] : "";
	templateId = BaseSerResp::GetString(json, "template_id", "");
	status = json.contains("status") ? json["status"].get<int>() : 0;
	planStarTime = json.contains("planned_start_time") ? json["planned_start_time"].get<int64_t>() : 0;
	planEndTime = json.contains("planned_end_time") ? json["planned_end_time"].get<int64_t>() : 0;
	startTime = json.contains("start_time") ? json["start_time"].get<int64_t>() : 0;
	endTime = json.contains("end_time") ? json["end_time"].get<int64_t>() : 0;
	proType = json.contains("process_type") ? json["process_type"].get<int>() : 0;
	remarks = BaseSerResp::GetString(json, "remarks", "");

	// 解析processes数组
	if (json.contains("processes") && json["processes"].is_array()) 
	{
		proVec.clear();
		for (const auto& process_item : json["processes"]) 
		{
			if (process_item.is_object()) 
			{
				SerProcess process;
				process.FromJson(process_item);
				proVec.push_back(process);
			}
		}
	}
}

UserDetailInfo::UserDetailInfo()
{

}

void UserDetailInfo::Clear()
{
	id = "";
	phoneNum = "";
	name.clear();
	appellation.clear();
	avatar.clear();
	gender = 0;
	country.clear();
	bgImg.clear();
	signature.clear();
	status = 0;
	level = 0;
	tags.clear();
}

void UserDetailInfo::FromJson(const nlohmann::json& json)
{
	id = json.contains("id") ? json["id"].get<std::string>() : "";
	phoneNum = json.contains("phone_number") ? json["phone_number"].get<std::string>() : 0;
	name = BaseSerResp::GetString(json, "name", "");
	appellation = BaseSerResp::GetString(json, "appellation", "");
	avatar = BaseSerResp::GetString(json, "avatar", "");
	gender = json.contains("gender") ? json["gender"].get<int>() : 0;
	country = BaseSerResp::GetString(json, "country", "");
	bgImg = BaseSerResp::GetString(json, "bg_image", "");
	signature = BaseSerResp::GetString(json, "signature", "");
	status = json.contains("status") ? json["status"].get<int>() : 0;
	level = json.contains("level") ? json["level"].get<int>() : 0;

	// 解析tags数组
	if (json.contains("tags") && json["tags"].is_array()) {
		tags.clear();
		for (const auto& tag : json["tags"]) 
		{
			if (tag.is_string()) {
				tags.push_back(BaseSerResp::GetString(tag, "", ""));
			}
		}
	}
}

DevSerItem::DevSerItem()
{

}

void DevSerItem::Clear()
{
	orderId.clear();
	itemId.clear();
	skuId = 0;
	orderType = 0;
	orderStatus = 0;
	quantity = 0;
	orderAmount = 0;
	createTime = 0;
	service.Clear();
	//sku.Clear();
	//user.Clear();
}

void DevSerItem::FromJson(const nlohmann::json& json)
{
	orderId = BaseSerResp::GetString(json, "order_id", "");
	itemId = BaseSerResp::GetString(json, "item_id", "");
	skuId = json.contains("sku_id") ? json["sku_id"].get<int64_t>() : 0;
	orderType = json.contains("order_type") ? json["order_type"].get<int>() : 0;
	orderStatus = json.contains("order_status") ? json["order_status"].get<int>() : 0;
	quantity = json.contains("quantity") ? json["quantity"].get<int>() : 0;
	orderAmount = json.contains("order_amount") ? json["order_amount"].get<int>() : 0;
	createTime = json.contains("create_time") ? json["create_time"].get<int64_t>() : 0;


	// 解析service对象
	if (json.contains("service") && json["service"].is_object()) 
	{
		service.FromJson(json["service"]);
	}

	//// 解析sku对象
	//if (json.contains("sku") && json["sku"].is_object())
	//{
	//	sku.FromJson(json["sku"]);
	//}

	//// 解析user对象
	//if (json.contains("user") && json["user"].is_object()) 
	//{
	//	user.FromJson(json["user"]);
	//}
}


void DevSereMapResp::FromJosn(const nlohmann::json& json)
{
	BaseSerResp::FromJosn(json);

	refresh_time = json.contains("refresh_time") ? json["refresh_time"].get<int64_t>() : 0;

	// 解析device_service_map对象
	if (json.contains("device_service_map") && json["device_service_map"].is_object()) 
	{
		devSerMap.clear();
		for (auto it = json["device_service_map"].begin(); it != json["device_service_map"].end(); ++it) 
		{
			std::string device_id = it.key();
			if (it.value().is_object()) 
			{
				DevSerItem item;
				item.FromJson(it.value());
				devSerMap[device_id] = item;
			}
		}
	}
}

void DevSereMapResp::Clear()
{
	devSerMap.clear();
	refresh_time = 0;
}