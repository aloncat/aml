//∙AML
// Copyright (C) 2016-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <exception>
#include <string_view>

namespace util {

//--------------------------------------------------------------------------------------------------------------------------------
class EGeneric : public std::exception
{
public:
	EGeneric() = default;
	explicit EGeneric(std::string_view msg) noexcept;
	EGeneric(const EGeneric& that) noexcept;
	virtual ~EGeneric() noexcept override;

	virtual const char* what() const noexcept override;
	virtual const char* ClassName() const noexcept = 0;

	EGeneric& operator =(const EGeneric& that) noexcept;

private:
	void Tidy() noexcept;
	// Выделяет массив необходимого размера, копирует в него size
	// символов строки str и возвращает указатель на этот массив
	static const char* CopyString(const char* str, size_t size);
	// Выделяет массив необходимого размера, копирует в него
	// строку str и возвращает указатель на этот массив
	static const char* CopyString(const char* str);

private:
	const char* m_What = nullptr;
};

//--------------------------------------------------------------------------------------------------------------------------------
#define AML_EXCEPTION(NAME, ANCESTOR) \
public: \
	NAME() noexcept : ANCESTOR(#NAME) {} \
	explicit NAME(std::string_view msg) noexcept : ANCESTOR(msg) {} \
	virtual const char* ClassName() const noexcept override { return "class util::" #NAME; }

//--------------------------------------------------------------------------------------------------------------------------------
class ELogic : public EGeneric
{
	AML_EXCEPTION(ELogic, EGeneric)
};

//--------------------------------------------------------------------------------------------------------------------------------
class EAssertion : public ELogic
{
	AML_EXCEPTION(EAssertion, ELogic)
};

//--------------------------------------------------------------------------------------------------------------------------------
class EHalt : public ELogic
{
	AML_EXCEPTION(EHalt, ELogic)
};

//--------------------------------------------------------------------------------------------------------------------------------
class ERuntime : public EGeneric
{
	AML_EXCEPTION(ERuntime, EGeneric)
};

} // namespace util
