#pragma once

template <typename T>
class CSingleton 
{
public:
	// ��ȡ����ʵ�����̰߳�ȫ��
	static T& GetInstance() 
	{
		static T instance;
		return instance;
	}

	// ���ÿ�������͸�ֵ����
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
	// �������캯������ֹ�ⲿʵ����
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
