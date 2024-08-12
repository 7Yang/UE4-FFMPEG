// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

DECLARE_DELEGATE_OneParam(Uint8EncodeDelegate,   uint8*)
DECLARE_DELEGATE_OneParam(DoubleEncodeDelegate, double*)

template<typename DataType>
class FEncodeData
{
public:
    FEncodeData();
	~FEncodeData();

    DataType* GetData();
	void InitializeData(int size);
	void SetEncodeData(DataType* Src);

private:
	int        DataSize;
    DataType*  DataMemory;
};

template<typename DataType>
class UCircleQueue
{
public:
	UCircleQueue();
	~UCircleQueue();

	bool IsFull();
	bool IsEmpty();
    void Resize(int DataSize);
	void Init(int QueueLength, int DataSize);
	bool InsertEncodeData(DataType* Data);
	bool PrcessEncodeData();

    Uint8EncodeDelegate  EncodeDelegateUint8;
	DoubleEncodeDelegate EncodeDelegateDouble;

private:
	uint32 QueueNum;
	uint32 QueueHead;
	uint32 QueueTail;
	uint32 QueueFreeNum;

    FEncodeData<DataType>* Queue;
};



