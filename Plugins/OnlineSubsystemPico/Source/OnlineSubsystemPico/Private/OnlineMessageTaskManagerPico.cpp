// Copyright 2022 Pico Technology Co., Ltd.All rights reserved.
// This plugin incorporates portions of the Unreal® Engine. Unreal® is a trademark or registered trademark of Epic Games, Inc.In the United States of America and elsewhere.
// Unreal® Engine, Copyright 1998 – 2022, Epic Games, Inc.All rights reserved.


#include "OnlineMessageTaskManagerPico.h"
#include "OnlineSubsystemPicoPrivate.h"
#include "PPF_Message.h"

FString FOnlineAsyncTaskPico::ToString() const
{
    FString Title = TEXT("RequestID:");
    FString RequestStr = Title + FString::Printf(TEXT("%llu"), RequestId);
    return RequestStr;
}

FString FOnlineAsyncEventPico::ToString() const
{
#if PLATFORM_ANDROID
    if (MessageHandle)
    {
        auto MessageType = ppf_Message_GetType(MessageHandle);
        int32 MessageTypeId = static_cast<int32>(MessageType);
        FString MesageTypeIdStr = FString::FromInt(MessageTypeId);
        FString Title = TEXT("Message Type:") + MesageTypeIdStr;
        return Title;
    }
    else
    {
        return TEXT("Message Error");
    }
#endif
    return FString();
}

void FOnlineAsyncTaskPico::Tick()
{
#if PLATFORM_ANDROID
    if (RequestId != ppfMessageType_User_GetAccessToken)
    {
        if (GetElapsedTime() >= 10.f)
        {
            bIsComplete = true;
            bWasSuccessful = false;
        }
    }


#endif
}

void FOnlineAsyncTaskPico::Finalize()
{
    UE_LOG_ONLINE(Log, TEXT("%s Task Finalize in %f seconds "), *ToString(), GetElapsedTime());
}

void FOnlineAsyncTaskPico::TriggerDelegates()
{
}


void FOnlineAsyncTaskPico::TaskReceiveMessage(ppfMessageHandle InMessageHandle, bool InbIsError)
{
    if (InMessageHandle)
    {
        bIsComplete = true;
        bWasSuccessful = true;
        MessageHandle = InMessageHandle;
        bIsError = InbIsError;
    }
    else
    {
        bIsComplete = true;
        bWasSuccessful = false;
        UE_LOG_ONLINE(Log, TEXT("Wrong Message Handle return!"));
    }
#if PLATFORM_ANDROID
    if (bWasSuccessful)
    {
        Delegate.ExecuteIfBound(MessageHandle, bIsError);
        Delegate.Unbind();
        ppf_FreeMessage(MessageHandle);
        MessageHandle = nullptr;
    }
#endif
}

void FOnlineAsyncTaskManagerPico::OnlineTick()
{
}

void FOnlineAsyncTaskManagerPico::TickTask()
{
#if PLATFORM_ANDROID
    for (;;)
    {
        ppfMessageHandle MessageHandle = ppf_PopMessage();
        if (!MessageHandle)
        {
            break;
        }
        UE_LOG_ONLINE(Log, TEXT("OnlineTick Receive Message !"));
        bool bIsError = ppf_Message_IsError(MessageHandle);
        ppfRequest RequestId = ppf_Message_GetRequestID(MessageHandle);
        UE_LOG_ONLINE(Log, TEXT("Receive request id: %llu!"), RequestId);

        if (RequestTaskMap.Contains(RequestId))
        {
            auto Item = RequestTaskMap[RequestId];
            Item->TaskReceiveMessage(MessageHandle, bIsError);
            delete Item;
            Item = nullptr;
            RequestTaskMap.Remove(RequestId);
            break;
        }

        ppfMessageType MessageType = ppf_Message_GetType(MessageHandle);
        if (NotificationMap.Contains(MessageType))
        {
            UE_LOG_ONLINE(Log, TEXT("Receive MessageTypeID: %i"), static_cast<int32>(MessageType));
            FOnlineAsyncEventPico* NewEvent = new FOnlineAsyncEventPico(PicoSubsystem, MessageHandle, bIsError, NotificationMap[MessageType]);
            NewEvent->TriggerDelegates();
            delete NewEvent;
            NewEvent = nullptr;
            break;
        }
        ppf_FreeMessage(MessageHandle);
    }
#endif
}

void FOnlineAsyncTaskManagerPico::CollectedRequestTask(ppfRequest Request, FOnlineAsyncTaskPico* InTask)
{
    UE_LOG_ONLINE(Log, TEXT("Send request id: %i"), static_cast<int32>(Request));
    RequestTaskMap.Add(Request, InTask);
}

FPicoMulticastMessageOnCompleteDelegate& FOnlineAsyncTaskManagerPico::GetOrAddNotifyDelegate(ppfMessageType MessageType)
{
    return NotificationMap.FindOrAdd(MessageType);
}

void FOnlineAsyncTaskManagerPico::RemoveNotifyDelegate(ppfMessageType MessageType, const FDelegateHandle& Delegate)
{
    NotificationMap.FindOrAdd(MessageType).Remove(Delegate);
}

