// Fill out your copyright notice in the Description page of Project Settings.

#include "EncodeData.h"
#include <type_traits>

template<typename DataType>
FEncodeData<DataType>::FEncodeData() 
    : DataMemory(nullptr)
{ 
}

template<typename DataType>
FEncodeData<DataType>::~FEncodeData()
{
	if (DataMemory) FMemory::Free(DataMemory);
}

template<typename DataType>
void FEncodeData<DataType>::InitializeData(int Size)
{
	DataSize   = Size;
	DataMemory = (DataType*)FMemory::Realloc(DataMemory, Size);
}

template<typename DataType>
void FEncodeData<DataType>::SetEncodeData(DataType* Src)
{	
	FMemory::StreamingMemcpy(DataMemory, Src, DataSize);	
}

template<typename DataType>
DataType* FEncodeData<DataType>::GetData()
{
	return DataMemory != nullptr ? DataMemory : nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DataType>
UCircleQueue<DataType>::UCircleQueue():
	Queue(nullptr)
{
	QueueHead = 0;
	QueueTail = 0;
}

template<typename DataType>
UCircleQueue<DataType>::~UCircleQueue()
{
	delete[] Queue;
}

template<typename DataType>
void UCircleQueue<DataType>::Resize(int DataSize)
{
    checkf(QueueNum, TEXT("must Init() before!"));
    for (uint32 i = 0; i < QueueNum; ++i)
    {
        Queue[i].InitializeData(DataSize);
    }
}

template<typename DataType>
void UCircleQueue<DataType>::Init(int QueueLen, int DataSized)
{
	QueueNum     = QueueLen;
	QueueFreeNum = QueueNum;
	Queue        = new FEncodeData<DataType>[QueueNum];
	for (uint32 i = 0; i < QueueNum; ++i)
	{
		Queue[i].InitializeData(DataSized);
	}
}

template<typename DataType>
bool UCircleQueue<DataType>::InsertEncodeData(DataType* NewData)
{
	if (IsFull()) return false;

	Queue[QueueTail].SetEncodeData(NewData);
	--QueueFreeNum;
	QueueTail = (QueueTail + 1) % QueueNum;

	return true;
}

template<typename DataType>
bool UCircleQueue<DataType>::PrcessEncodeData()
{
	if (IsEmpty()) return false;

    if (std::is_same<DataType, uint8>())
        EncodeDelegateUint8 .ExecuteIfBound(( uint8*)Queue[QueueHead].GetData());
    else if (std::is_same<DataType, double>())
        EncodeDelegateDouble.ExecuteIfBound((double*)Queue[QueueHead].GetData());

	QueueHead = (QueueHead + 1) % QueueNum;
	++QueueFreeNum;

	return true;
}

template<typename DataType>
bool UCircleQueue<DataType>::IsFull()
{
	return QueueFreeNum == 0 ? true : false;
}

template<typename DataType>
bool UCircleQueue<DataType>::IsEmpty()
{
	return QueueFreeNum == QueueNum ? true : false;
}