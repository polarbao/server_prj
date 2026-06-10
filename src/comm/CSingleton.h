#pragma once

template <typename T>
class CSingleton 
{
public:
	// 获取单例实例（线程安全）
	static T& GetInstance() 
	{
		static T instance;
		return instance;
	}

	// 禁用拷贝构造和赋值操作
	/** 
	*  @brief       brief 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
	CSingleton(const CSingleton&) = delete;

	/** 
	*  @brief       brief 
	*  @param[in]    
	*  @param[out]   
	*  @return                    
	*/
	CSingleton& operator=(const CSingleton&) = delete;

protected:
	// 保护构造函数，防止外部实例化
	/**
	*  @brief       brief
	*  @param[in]
	*  @param[out]
	*  @return
	*/
	CSingleton() = default;

	/**
	*  @brief       brief
	*  @param[in]
	*  @param[out]
	*  @return
	*/
	virtual ~CSingleton() = default;
};

//#ifndef CSINGLETON_H
//#define CSINGLETON_H
//#endif // CSINGLETON_H