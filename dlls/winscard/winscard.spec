@ stub ClassInstall32
@ stub SCardAccessNewReaderEvent
@ stub SCardReleaseAllEvents
@ stub SCardReleaseNewReaderEvent
@ stdcall SCardAccessStartedEvent()
@ stdcall SCardAddReaderToGroupA(long str str)
@ stdcall SCardAddReaderToGroupW(long wstr wstr)
@ stdcall SCardBeginTransaction(long) wine_SCardBeginTransaction
@ stdcall SCardCancel(long) wine_SCardCancel
@ stdcall SCardConnectA(long ptr long long long long)
@ stdcall SCardConnectW(long ptr long long long long)
@ stdcall SCardControl(long long ptr long ptr long ptr) wine_SCardControl
@ stdcall SCardDisconnect(long long) wine_SCardDisconnect
@ stdcall SCardEndTransaction(long long) wine_SCardEndTransaction
@ stdcall SCardEstablishContext(long ptr ptr ptr) wine_SCardEstablishContext
@ stub SCardForgetCardTypeA
@ stub SCardForgetCardTypeW
@ stub SCardForgetReaderA
@ stub SCardForgetReaderGroupA
@ stub SCardForgetReaderGroupW
@ stub SCardForgetReaderW
@ stdcall SCardFreeMemory(long ptr) wine_SCardFreeMemory
@ stdcall SCardGetAttrib(long long ptr ptr) wine_SCardGetAttrib
@ stub SCardGetCardTypeProviderNameA
@ stub SCardGetCardTypeProviderNameW
@ stub SCardGetProviderIdA
@ stub SCardGetProviderIdW
@ stdcall SCardGetStatusChangeA(long long ptr long)
@ stdcall SCardGetStatusChangeW(long long ptr long)
@ stub SCardIntroduceCardTypeA
@ stub SCardIntroduceCardTypeW
@ stub SCardIntroduceReaderA
@ stub SCardIntroduceReaderGroupA
@ stub SCardIntroduceReaderGroupW
@ stub SCardIntroduceReaderW
@ stdcall SCardIsValidContext(long) wine_SCardIsValidContext
@ stdcall SCardListCardsA(long ptr ptr long str long)
@ stub SCardListCardsW
@ stub SCardListInterfacesA
@ stub SCardListInterfacesW
@ stdcall SCardListReaderGroupsA(long ptr long)
@ stdcall SCardListReaderGroupsW(long ptr long)
@ stdcall SCardListReadersA(long str ptr ptr)
@ stdcall SCardListReadersW(long wstr ptr ptr)
@ stub SCardLocateCardsA
@ stub SCardLocateCardsByATRA
@ stub SCardLocateCardsByATRW
@ stub SCardLocateCardsW
@ stdcall SCardReconnect(long long long long ptr) wine_SCardReconnect
@ stdcall SCardReleaseContext(long) wine_SCardReleaseContext
@ stdcall SCardReleaseStartedEvent()
@ stub SCardRemoveReaderFromGroupA
@ stub SCardRemoveReaderFromGroupW
@ stdcall SCardSetAttrib(long long ptr long) wine_SCardSetAttrib
@ stub SCardSetCardTypeProviderNameA
@ stub SCardSetCardTypeProviderNameW
@ stub SCardState
@ stdcall SCardStatusA (long str long long long ptr long )
@ stdcall SCardStatusW (long wstr long long long ptr long )
@ stdcall SCardTransmit(long ptr ptr long ptr ptr ptr) wine_SCardTransmit
@ extern g_rgSCardRawPci
@ extern g_rgSCardT0Pci
@ extern g_rgSCardT1Pci
