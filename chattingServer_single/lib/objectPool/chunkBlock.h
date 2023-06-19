#pragma once

#if _WIN64 || __x86_64__ || __ppc64__
#define FLAG				0xFFFF000000000000
#define CHECK				0xCDCD000000000000
#define	PTRSIZEINT			__int64 

#else
#define FLAG				0x80000000
#define CHECK				0x80000000
#define	PTRSIZEINT			__int32
#endif


//#pragma pack(push, 1)
template <typename T>
struct  memoryBlock
{
	memoryBlock() {
	}
	//template <typename... param>
	//memoryBlock(memoryBlock* _next, param... args)
	//	: nodeLink(nullptr), data(args ...), next(_next) {
	//}
	~memoryBlock() {
	}
	// ������ pool���� ������ ��带 �Ϸķ� ������ �ִ� ���Ḯ��Ʈ, ���� ������ ���� pool ���� linkPtr ���� nodeLink�� �����ϰ�, ���� ������ ����� �ּҸ� pool::linkPtr�� �����Ѵ�.
	memoryBlock* nodeLink;
	T data;
	// �ܺο� �����ִ� ���� ������ pool �ּҸ�, ���ο� �ִ� ���� �ڽ��� �Ҵ� �� ���� top�� �� ����� �ּҸ� ����������
	memoryBlock* next;
};
//#pragma pack(pop)

template <typename T>
struct chunkBlock
{
	chunkBlock()
		: headNodePtr(nullptr), tailNodePtr(nullptr), nextChunkBlock(nullptr), lastUseTick(GetTickCount64()), maxSize(0), curSize(0) {
	}

	chunkBlock(int chunkSize)
		: headNodePtr(nullptr), tailNodePtr(nullptr), nextChunkBlock(nullptr), lastUseTick(GetTickCount64()), maxSize(chunkSize), curSize(chunkSize) {

		headNodePtr = new memoryBlock<T>();
		tailNodePtr = headNodePtr;
		tailNodePtr->next = nullptr;

		while (--chunkSize > 0)
		{
			memoryBlock<T>* temp = new memoryBlock<T>();
			temp->next = headNodePtr;
			headNodePtr = temp;
		}
	}
	~chunkBlock() {
		while (curSize-- > 0)
		{
			memoryBlock<T>* temp = headNodePtr;
			headNodePtr = headNodePtr->next;
			delete temp;
		}
	}

	void reAlloc() {
		int offset = maxSize - curSize;

		if (curSize == 0) {
			headNodePtr = new memoryBlock<T>();
			tailNodePtr = headNodePtr;
			offset -= 1;
			curSize += 1;
		}

		while (offset-- > 0)
		{
			memoryBlock<T>* temp = new memoryBlock<T>();
			temp->next = headNodePtr;
			headNodePtr = temp;

			++curSize;
		}
	}
	memoryBlock<T>* headNodePtr;
	memoryBlock<T>* tailNodePtr;
	chunkBlock* nextChunkBlock;
	unsigned long long int	lastUseTick;
	int						maxSize;
	int						curSize;
};

//#pragma pack(push, 1)
template <typename T>
struct alignMemoryBlock
{
	alignMemoryBlock(void* _freePtr)
		: freePtr(_freePtr), nodeLink(nullptr), next(nullptr) {
	}

	template <typename... param>
	alignMemoryBlock(void* _freePtr, param... args)
		: freePtr(_freePtr), nodeLink(nullptr), data(args ...), next(nullptr) {
	}
	//template <typename... param>
	//alignMemoryBlock(void* _freePtr, alignMemoryBlock* _next, param... args)
	//	: freePtr(_freePtr), data(args ...), next(nullptr) {
	//}

	~alignMemoryBlock() {
	}

	// ������ pool���� ������ ��带 �Ϸķ� ������ �ִ� ���Ḯ��Ʈ, ���� ������ ���� pool ���� linkPtr ���� nodeLink�� �����ϰ�, ���� ������ ����� �ּҸ� pool::linkPtr�� �����Ѵ�.
	alignMemoryBlock* nodeLink;
	T data;
	// �ܺο� �����ִ� ���� ������ pool �ּҸ�, ���ο� �ִ� ���� �ڽ��� �Ҵ� �� ���� top�� �� ����� �ּҸ� ����������
	alignMemoryBlock* next;
	// align ������ T data�� ���� �ּҸ� ĳ�ö��ο� ���߱� ������ ���� �޸� ����� �Ҵ�� �ּҰ� T data�� �ּ� - sizeof(alignMemoryBlock*) �� �ƴϴ�. ��������� �е��� �����ϴ��� �� �� ���� ������ ���� �޸𸮸� free �Ҷ� ����� freePtr�� ������ �д�.
	void* freePtr;
};
//#pragma pack(pop)