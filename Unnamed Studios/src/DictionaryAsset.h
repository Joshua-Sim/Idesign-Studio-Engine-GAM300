///////////////////////////////////////////////////////////////////////////////
///
///	@File  : DictionaryAsset.h
/// @Brief : Defines the Dictionary class, which manages a collection of words 
///          and prefixes loaded from files. Provides functionality for loading 
///          and accessing these words and prefixes for application usage.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_
#include "pch.h"
#include "JsonSerialize.h"

/**
 * @class Dictionary
 * @brief Manages a collection of words and prefixes, providing functions for
 *        loading from files and accessing stored data.
 */
class Dictionary
{
public:

	/**
	 * @brief Constructs a Dictionary object, loading data from specified files.
	 * @param dictionaryPath Path to the dictionary file.
	 * @param prefixPath Path to the prefix file.
	 */
	Dictionary(const std::string& dictionaryPath, const std::string& prefixPath);
	
	/**
	 * @brief Destructor for the Dictionary class.
	 */
	~Dictionary();
	
	/**
	 * @brief Loads words from a dictionary file.
	 * @param dictionaryPath Path to the dictionary file.
	 */
	void DeserializeDictionary(const std::string& dictionaryPath);
	
	/**
	 * @brief Loads prefixes from a prefix file.
	 * @param prefixPath Path to the prefix file.
	 */
	void DeserializePrefixes(const std::string& prefixPath);

	/**
	 * @brief Retrieves a random prefix from the loaded prefixes.
	 * @return A random prefix as a string.
	 */
	std::string UE_GetRandomPrefix();

	/**
	 * @brief Retrieves the loaded dictionary words.
	 * @return A constant reference to the set of dictionary words.
	 */
	const std::unordered_set<std::string>& GetWords() const { return dictionary; }

	/**
	 * @brief Retrieves the loaded prefixes.
	 * @return A constant reference to the vector of prefixes.
	 */
	const std::vector<std::string>& GetPrefixes() const { return prefixes; }

private:
	std::unordered_set<std::string> dictionary;                 // To store all the words from the dictionary.txt
	std::vector<std::string> prefixes;                          // To store the prefixes from prefix.txt
};

#endif // !_DICTIONARY_H_