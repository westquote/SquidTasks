#pragma once

#include "TextInput.h"

#include "TaskManager.h"
#include "TokenList.h"
#include "FunctionGuard.h"

#include <sstream>
#include <random>
#include <limits>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <tuple>

using namespace Squid;

// GAME IDEAS:
// - Implement achievements
// - Implement accuracy/dodge + crits to improve combat non-determinism
// - If max stage > the last stage, let the player know they have beaten the game!
// - Implement NG+ (reset all stats, game is harder, you keep your spellbook, and there is a True Last Boss at the end)
// - Implement "gold" stat
// - Lose gold when you rest (and extra if you swoon)
// - Implement basic shop for buying armor with gold
// - Make training cost gold (in addition to SP, possibly increasing by stat level)

// [TEXTGAME_ENABLE_PERIODIC_DEBUG]: Periodically print a debug snapshot of tasks running on the TaskManager
#ifdef SQUID_ENABLE_TASK_DEBUG
#define TEXTGAME_ENABLE_PERIODIC_DEBUG 0
#endif //SQUID_ENABLE_TASK_DEBUG

class TextGameDebugStackFormatter : public TaskDebugStackFormatter
{
	std::string Indent(int32_t in_indent) override
	{
		return TaskDebugStackFormatter::Indent(in_indent + 1); // Base indentation is 1, (instead of 0)
	}
};

// Periodically print a debug snapshot of tasks running on a TaskManager
Task<> PeriodicallyPrintDebug(float in_delay, TaskManager& in_taskMgr)
{
	TASK_NAME(__FUNCTION__);

	TextGameDebugStackFormatter formatter;
	while(true)
	{
		co_await WaitSeconds(in_delay);
		std::cout << "Currently running tasks:" << std::endl << in_taskMgr.GetDebugString(formatter) << std::endl;
	}
}

class TextGame
{
public:
	TextGame()
		: m_mersenne(m_randDev())
	{
		m_taskMgr.RunManaged(MainLoop());

#if TEXTGAME_ENABLE_PERIODIC_DEBUG
		m_taskMgr.RunManaged(PeriodicallyPrintDebug(10.0f, m_taskMgr));
#endif //TEXTGAME_ENABLE_PERIODIC_DEBUG
	}
	~TextGame()
	{
	}
	void Update()
	{
		m_taskMgr.Update();
	}
	bool IsGameOver() const
	{
		return m_isGameOver;
	}

private:
	// Random Numbers
	std::random_device m_randDev;
	std::mt19937 m_mersenne;
	std::uniform_real_distribution<float> m_randFloat = std::uniform_real_distribution<float>(0.0, 1.0);

	// Task Management
	TaskManager m_taskMgr;
	TextInput m_textInput;
	bool m_isGameOver = false;

	// Spell Archive
	struct Character;
	struct Spell
	{
		using tTaskFn = std::function<Task<>(const Spell&, Character&, Character&)>; // (Spell, Attacker, Defender)
		tTaskFn taskFn;
		char shortcut = 0;
		std::string name;
		int32_t mpCost = 5;
		float cooldown = 1.0;
		std::string desc;

		bool operator<(const Spell& in_other) const
		{
			return name < in_other.name;
		}
	};
	std::set<Spell> m_spellArchive;

	// Character Data
	struct Character
	{
		static constexpr int32_t SAVE_FORMAT_VERSION = 0;

		// Vital Stats
		std::string name;
		int32_t health = 20;
		int32_t maxHealth = 20;
		int32_t mana = 0;
		int32_t maxMana = 0;

		// Progression Stats
		int32_t xp = 0;
		int32_t level = 1;
		int32_t maxStage = 1;

		// Combat Stats
		int32_t strength = 1; // Base damage
		int32_t armor = 0; // Subtractive (prior to defense)
		int32_t defense = 0; // Applied as a percentage (after armor), 4% damage reduction per level
		int32_t speed = 0; // Reduces delay between attacks, 0.02sec reduction per level
		float baseAttackDelay = 1.0; // Base delay between attacks (in seconds)

		// Magic Stats
		int32_t intelligence = 1; // Base magic scalar
		std::map<char, Spell> spellBook;
		void LearnSpell(const Spell& in_spell)
		{
			spellBook[in_spell.shortcut] = in_spell;
		}

		// Skill Stats
		int32_t skillPoints = 0; // Training/studying requires 1 skill point
		int32_t maxSkillPoints = 0; // Maximum skill points to accumulate
		float skillPointRegenRate = 60.0; // Seconds that must pass to earn another skill point

		// Conditions
		struct Conditions
		{
			std::list<TaskHandle<>> conditionTasks;
			TokenList<> poisonTokens;
			TokenList<> regenTokens;
			TokenList<> hasteTokens;
			TokenList<> fortifyTokens;
			TokenList<> stunTokens;
		} conditions;

		// Helper functions
		void ClearConditions()
		{
			conditions = {};
		}

		// Save/Load
		void SaveToFile()
		{
			std::ofstream saveFile("saves/" + name + ".gqs");
			auto WriteToFile = [&saveFile](const auto& in_val) {
				saveFile.write((char*)&in_val, sizeof(in_val));
			};
			auto WriteStrToFile = [&saveFile, WriteToFile](const std::string& in_val) {
				WriteToFile((int32_t)in_val.size());
				saveFile.write(&in_val[0], in_val.size());
			};
			if(saveFile.is_open())
			{
				WriteToFile(SAVE_FORMAT_VERSION);
				WriteStrToFile(name);
				WriteToFile(health);
				WriteToFile(maxHealth);
				WriteToFile(mana);
				WriteToFile(maxMana);
				WriteToFile(xp);
				WriteToFile(level);
				WriteToFile(maxStage);
				WriteToFile(strength);
				WriteToFile(armor);
				WriteToFile(defense);
				WriteToFile(speed);
				WriteToFile(baseAttackDelay);
				WriteToFile(intelligence);
				WriteToFile(skillPoints);
				WriteToFile(maxSkillPoints);
				WriteToFile((int32_t)spellBook.size());
				for(const auto& spell : spellBook)
				{
					WriteStrToFile(spell.second.name);
				}
			}
		}
		bool LoadFromFile(TextGame* in_game)
		{
			std::ifstream saveFile("saves/" + name + ".gqs");
			auto ReadFromFile = [&saveFile](auto& out_val) {
				saveFile.read((char*)&out_val, sizeof(out_val));
			};
			auto ReadStrFromFile = [&saveFile, ReadFromFile](std::string& out_val) {
				int32_t strSize = 0;
				ReadFromFile(strSize);
				out_val.resize(strSize);
				saveFile.read(&out_val[0], strSize);
			};
			if(saveFile.is_open())
			{
				int32_t version = 0;
				ReadFromFile(version);
				ReadStrFromFile(name);
				ReadFromFile(health);
				ReadFromFile(maxHealth);
				ReadFromFile(mana);
				ReadFromFile(maxMana);
				ReadFromFile(xp);
				ReadFromFile(level);
				ReadFromFile(maxStage);
				ReadFromFile(strength);
				ReadFromFile(armor);
				ReadFromFile(defense);
				ReadFromFile(speed);
				ReadFromFile(baseAttackDelay);
				ReadFromFile(intelligence);
				ReadFromFile(skillPoints);
				ReadFromFile(maxSkillPoints);
				int32_t spellBookSize = 0;
				ReadFromFile(spellBookSize);
				while(spellBookSize-- > 0)
				{
					std::string spellName;
					ReadStrFromFile(spellName);
					auto spell = in_game->GetSpellByName(spellName);
					if(spell)
					{
						LearnSpell(spell.value());
					}
				}
				return true;
			}
			return false;
		}

		// Stats debug output
		std::string GetStatsString()
		{
			std::stringstream statsStr;
			statsStr << name << " - " << health << "/" << maxHealth << " HP";
			if(maxMana > 0)
			{
				statsStr << ", " << mana << "/" << maxMana << " MP";
			}
			if(maxSkillPoints > 0)
			{
				statsStr << ", " << skillPoints << " SP";
			}
			return statsStr.str();
		}
		std::string GetFullStatsString()
		{
			std::stringstream statsStr;
			statsStr << name << " - Level " << level << ", " << health << "/" << maxHealth << " HP";
			if(maxMana > 0)
			{
				statsStr << ", " << mana << "/" << maxMana << " MP";
			}
			if(maxSkillPoints > 0)
			{
				statsStr << ", " << skillPoints << " SP";
			}
			statsStr << ", " << xp << "/" << (level * level) << " XP";

			return statsStr.str();
		}
	};

	// Game Data
	struct GameData
	{
		using tWords = std::vector<std::vector<std::string>>;
		tWords words;
		std::vector<std::tuple<std::string, std::string>> riddles;
		std::vector<std::tuple<std::string, std::vector<std::string>, std::vector<std::string>>> nyms;

		void LoadData(TextGame* in_game)
		{
			// Load words list
			std::string line;
			std::ifstream wordsFile("gamedata/words.txt");
			words.resize(16);
			for(auto& wordList : words)
			{
				wordList.reserve(100);
			}
			while(std::getline(wordsFile, line))
			{
				line.erase(remove_if(line.begin(), line.end(), std::isspace), line.end());
				size_t len = line.size() - 1;
				len = len >= words.size() ? words.size() - 1 : len;
				auto& wordList = words[len];
				wordList.push_back(line);
			}

			// Load antonyms list
			std::ifstream nymsFile("gamedata/nyms.csv");
			while(std::getline(nymsFile, line))
			{
				auto wordStart = 0;
				auto wordEnd = line.find('\t');
				auto synEnd = line.find('\t', wordEnd + 1);
				auto word = line.substr(wordStart, wordEnd);
				auto synLine = line.substr(wordEnd + 1, synEnd - (wordEnd + 1));
				auto antLine = line.substr(synEnd + 1);
				auto syns = Split(synLine, ", ");
				auto ants = Split(antLine, ", ");
				nyms.push_back({ word, syns, ants });
			}

			// Load riddles list
			bool encodeRiddles = false;
			std::ifstream riddlesFile(encodeRiddles ? "gamedata/riddles.csv" : "gamedata/riddles_enc.csv");
			while(std::getline(riddlesFile, line))
			{
				auto riddleStart = line.find('\"') + 1;
				auto riddleEnd = line.find('\"', riddleStart);
				auto answerStart = line.find(',', riddleEnd + 1) + 1;
				auto riddle = line.substr(riddleStart, riddleEnd - riddleStart);
				auto answer = line.substr(answerStart);
				answer.erase(remove_if(answer.begin(), answer.end(), [](char c) { return c == '.'; }), answer.end());
				if(!encodeRiddles)
				{
					riddle = Rot13(riddle);
					answer = Rot13(answer);
				}
				answer.erase(remove_if(answer.begin(), answer.end(), std::isspace), answer.end());
				riddles.push_back({ riddle, answer });
			}
			if(encodeRiddles)
			{
				std::ofstream riddlesEncFile("gamedata/riddles_enc.csv");
				for(const auto& riddleTuple : riddles)
				{
					std::string riddle, answer;
					std::tie(riddle, answer) = riddleTuple;
					riddlesEncFile << '\"' << Rot13(riddle) << "\"," << Rot13(answer) << std::endl;
				}
			}
		}
	};
	GameData m_data;

	// Main game loop
	Task<> MainLoop()
	{
		TASK_NAME(__FUNCTION__);

		// Generate spell archive
		GenerateSpellArchive();

		// Load game data
		m_data.LoadData(this);

		// Create player character
		Character player;
		player.maxSkillPoints = 3;

		// Intro
		bool showIntro = true;
		if(showIntro)
		{
			co_await Teletype("*** GeneriQuest 0.1 ***");
			NewLine();
			co_await Teletype("What is your name?", 0.0);
			player.name = co_await WaitForInput();
			NewLine();

			if(!player.LoadFromFile(this))
			{
				co_await Teletype(std::string("Welcome, ") + player.name + ", to GeneriQuest!");
				player.SaveToFile();
			}
			else
			{
				co_await Teletype(std::string("Welcome back, ") + player.name + "!");
			}
		}
		else
		{
			player.name = "Player";
		}

		// Start task that earns skill points
		auto skillPointRegen = m_taskMgr.Run([](Character& player) -> Task<> {
			TASK_NAME("SkillPointRegenLambda");

			while(true)
			{
				co_await WaitSeconds(player.skillPointRegenRate);
				if(player.skillPoints < player.maxSkillPoints)
				{
					player.skillPoints += 1;
				}
			}
		}(player));

		// Action loop
		auto saveGuard = MakeFnGuard([&player] { player.SaveToFile(); });
		while(!m_isGameOver)
		{
			co_await Teletype(player.GetFullStatsString());
			NewLine();

			co_await MultipleChoice("What would you like to do next?", {
				{ "Battle", [this, &player]() -> Task<> { co_await Mode_Battle(player); } },
				{ "Train", [this, &player]() -> Task<> { co_await Mode_Practice(player); } },
				{ "Sleep", [this, &player]() -> Task<> { co_await Mode_Sleep(player); } },
				{ "Quit", [this]() -> Task<> { co_await ConfirmQuit(); } },
			});

			// Auto-save
			player.SaveToFile();
		}
	}

	// Modes
	Task<> Mode_Battle(Character& player)
	{
		TASK_NAME(__FUNCTION__);

		// Cannot battle if health is 0
		if(player.health <= 0)
		{
			co_await Teletype("You are too wounded to battle. Get some sleep!");
			co_return;
		}

		// Select stage
		int32_t stage = 1;
		if(player.maxStage > 1)
		{
			stage = 0; // Invalidate stage
			while(stage < 1 || stage > player.maxStage)
			{
				std::stringstream promptStr;
				promptStr << "Select a stage [1-" << player.maxStage << "]:";
				co_await Teletype(promptStr.str(), 0.0);
				auto stageStr = co_await WaitForInput();
				stage = StrToInt(stageStr).value_or(0);
			}
		}

		// Simple combat simulator against an enemy (scaled by stage)
		Character enemy = GetRandomEnemy(stage);

		// Engage in combat until enemy or player have 0 HP
		{
			std::stringstream combatStr;
			combatStr << "You encounter a monster! (" << enemy.GetStatsString() << ")";
			co_await Teletype(combatStr.str(), 0.0);

			auto enemyCombatTask = m_taskMgr.Run(Combat(enemy, player));
			auto playerCombatTask = m_taskMgr.Run(Combat(player, enemy));
			auto playerMagicTask = m_taskMgr.Run(PlayerMagic(player, enemy));
			while(enemy.health > 0 && player.health > 0)
			{
				co_await Suspend();
			}
		}

		// Give XP for a successful completion (including level)
		if(player.health > 0)
		{
			NewLine();
			Teletype("Victory!");
			auto xpLevel = player.level - 1;
			xpLevel = xpLevel < 0 ? 0 : xpLevel;
			auto xpEarned = (stage * stage) - (xpLevel * xpLevel) + stage;
			if(xpEarned > 0)
			{
				player.xp += xpEarned;

				std::stringstream combatStr;
				combatStr << "Gained " << xpEarned << " XP";
				co_await Teletype(combatStr.str());
			}
			else
			{
				co_await Teletype("No XP earned! (Try a higher stage)");
			}
			while(player.xp >= player.level * player.level)
			{
				co_await Teletype("Level Up!");
				++player.level;

				// Unlocked magic
				if(player.level == 3)
				{
					co_await Teletype("You feel your mind awakening...");
					NewLine();
					co_await Teletype("!!! You can now train to learn magic!");
					NewLine();
					player.mana += 5;
					player.maxMana += 5;
				}
				else
				{
					player.maxMana += player.level - 2;
				}

				// Give level stats bonuses
				player.maxHealth += player.level - 1;
				player.strength += 1;
				player.defense += 1;
				player.speed += 1;
			}

			std::stringstream xpToNextLevelStr;
			xpToNextLevelStr << (player.level * player.level) - player.xp << " XP to reach next level";
			co_await Teletype(xpToNextLevelStr.str());

			// Increment max stage if this is our highest stage
			if(player.maxStage == stage)
			{
				co_await Teletype("Max stage increased!");
				++player.maxStage;
			}
		}
		else
		{
			co_await Teletype("SWOON! (Rest up to battle again)");
		}
	}
	Task<> Mode_Practice(Character& player)
	{
		TASK_NAME(__FUNCTION__);

		auto practiceType = "train";

		// Cannot continue if health is 0
		if(player.health <= 0)
		{
			co_await Teletype(std::string("You are too wounded to ") + practiceType + ". Get some sleep!");
			co_return;
		}

		// Cannot continue if out of skill points
		if(player.skillPoints <= 0)
		{
			co_await Teletype(std::string("You can't ") + practiceType + " any more right now! Come back later...");
			co_return;
		}

		// Quick format helper
		auto FormatStat = [](const std::string& in_name, int32_t in_value)
		{
			std::stringstream statStr;
			statStr << in_name << " [" << in_value << "]";
			return statStr.str();
		};

		// Select practice mode
		auto practiceComplete = false;
		while(!practiceComplete && player.skillPoints > 0)
		{
			std::stringstream statusStr;
			statusStr << "You have " << player.skillPoints << " SP left";
			co_await Teletype(statusStr.str(), 0.0, 0.0);
			std::vector<Choice> choices = {
					{ FormatStat("Strength", player.strength), [this, &player]() -> Task<> { co_await Practice_Strength(player); } },
					{ FormatStat("Defense", player.defense), [this, &player]() -> Task<> { co_await Practice_Defense(player); } },
					{ FormatStat("Speed", player.speed), [this, &player]() -> Task<> { co_await Practice_Speed(player); } },
			};
			if(player.maxMana > 0) // If player has unlocked magic
			{
				choices.insert(choices.end(), {
					{ FormatStat("Magic", player.intelligence), [this, &player]() -> Task<> { co_await Practice_Magic(player); } },
					{ FormatStat("Spells", (int32_t)player.spellBook.size()), [this, &player]() -> Task<> { co_await Practice_Spells(player); } },
				});
			}
			choices.push_back({ "End Training", [&practiceComplete]() -> Task<> { practiceComplete = true; co_return; } });
			co_await MultipleChoice("What would you like to work on?", choices);
		}
		if(player.skillPoints <= 0)
		{
			co_await Teletype(std::string("No skill points remaining. Come back later to ") + practiceType + " more!", 0.0, 0.0);
		}
	}
	Task<> Mode_Sleep(Character& player)
	{
		TASK_NAME(__FUNCTION__);

		co_await Teletype("You get a good night's sleep - HP + MP fully-restored!", 2.0);
		player.health = player.maxHealth;
		player.mana = player.maxMana;
	}

	// Skill Training Mini-Games
	std::string GetRandomWord(int32_t in_minLen = 1, int32_t in_maxLen = -1)
	{
		int32_t maxWordLen = (int32_t)m_data.words.size();
		in_maxLen = in_maxLen < 0 || in_maxLen > maxWordLen ? maxWordLen :
					in_maxLen < 1 ? 1 :
					in_maxLen;
		int32_t lenWords = RandInRange(in_minLen, in_maxLen);
		const auto& wordList = m_data.words[lenWords - 1];
		auto word = wordList[RandInt() % wordList.size()];
		return word;
	}
	Task<bool> WaitForInputAndCheck(const std::vector<std::string>& in_words, float in_timeout, const std::string& in_successText, const std::string& in_failureText, const std::string& in_slowText)
	{
		TASK_NAME(__FUNCTION__);

		auto input = co_await Timeout(WaitForInput(), in_timeout);
		NewLine();
		if(input)
		{
			for(const auto& word : in_words)
			{
				if(ToLower(input.value()) == ToLower(word))
				{
					co_await Teletype(in_successText, 2.0);
					NewLine();
					co_return true;
				}
			}
			co_await Teletype(in_failureText, 2.0);
			NewLine();
		}
		else
		{
			co_await Teletype(in_slowText, 2.0);
		}
		co_return false;
	}
	Task<> Practice_Strength(Character& player)
	{
		TASK_NAME(__FUNCTION__);

		--player.skillPoints;

		// Gain 1 point of Strength (type difficult word on a timer)
		co_await Teletype("Get ready...", 3.0f);
		auto wordLength = Lookup(player.strength, std::vector<int32_t>{3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 9, 9, 10, 11, 12, 13});
		auto word = GetRandomWord(wordLength, wordLength + 3);
		co_await Teletype(std::string("QUICK! Type the word '") + word + "'!", 0.0f);
		auto timePerWord = wordLength * 0.25f;
		timePerWord = timePerWord < 2.0f ? 2.0f : timePerWord;

		if(co_await WaitForInputAndCheck({ word }, timePerWord,
			"Good hustle! You have grown stronger!",
			"Mediocre... Come back when you're serious about getting swole.",
			"TOO SLOW! Training failed."))
		{
			++player.strength;
		}
	}
	Task<> Practice_Defense(Character& player)
	{
		TASK_NAME(__FUNCTION__);

		--player.skillPoints;

		// Gain 1 point of Defense (type word in reverse on a timer)
		co_await Teletype("Get ready...", 3.0);

		auto wordLength = Lookup(player.defense, std::vector<int32_t>{3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 9, 9, 10, 11, 12, 13});
		auto word = GetRandomWord(wordLength, wordLength + 3);
		co_await Teletype(std::string("QUICK! Type the word '") + word + "' BACKWARDS!", 0.0f);
		auto timePerWord = wordLength * 1.25f;
		timePerWord = timePerWord < 5.0f ? 5.0f : timePerWord;
		std::reverse(word.begin(), word.end()); // Reverse the word

		if(co_await WaitForInputAndCheck({ word }, timePerWord,
			"Expertly done! Your defensive abilities have improved!",
			"No good. You need to work harder at deflecting these attacks!",
			"TOO SLOW! Training failed."))
		{
			++player.defense;
		}
	}
	Task<> Practice_Speed(Character& player)
	{
		TASK_NAME(__FUNCTION__);

		--player.skillPoints;

		// Gain 1 point of Speed (type N words on a timer)
		co_await Teletype("Get ready...", 3.0f);
		std::string words;
		auto l = 1.66f; // long
		auto m = 1.5f; // medium
		auto s = 1.33f; // short
		int32_t numWords = Lookup(player.speed, std::vector<int32_t>{3, 3, 3, 4, 4, 5, 5, 5, 6, 6, 7});
		auto wordLength  = Lookup(player.speed, std::vector<int32_t>{3, 3, 3, 3, 4, 4, 5, 5, 5, 6, 6});
		auto timePerWord = Lookup(player.speed, std::vector<float> {l, l, l, l, m, m, m, m, s, s, s});
		for(auto i = 0; i < numWords; ++i)
		{
			if(i > 0)
			{
				words += " ";
			}
			auto word = GetRandomWord(wordLength, wordLength + 1);
			words += word;
		}
		co_await Teletype("QUICK! Type all of these words:", 0.25f);
		co_await Teletype(words, 0.0f);

		if(co_await WaitForInputAndCheck({ words }, numWords * timePerWord,
			"Quick as lightning! Your training has made your faster!",
			"You are fast... but you must also be accurate! Try again later.",
			"TOO SLOW! Training failed."))
		{
			++player.speed;
		}
	}
	Task<> Practice_Magic(Character& player)
	{
		TASK_NAME(__FUNCTION__);

		--player.skillPoints;

		// Gain 1 point of Intelligence (type synonym or antonym of word on a timer)
		co_await Teletype("Get ready...", 3.0f);
		std::string word;
		std::vector<std::string> syns, ants;
		std::tie(word, syns, ants) = m_data.nyms[RandInt() % m_data.nyms.size()];
		bool opposite = RandInt() % 2 == 0;
		if(opposite && ants.size() == 0)
		{
			opposite = false;
		}
		std::string prompt;
		std::string targetWord;
		if(opposite)
		{
			prompt = std::string("QUICK! Which of these is the opposite of '") + word + "'?";
			targetWord = ants[RandInt() % ants.size()];
		}
		else
		{
			prompt = std::string("QUICK! Which of these is another word for '") + word + "'?";
			targetWord = syns[RandInt() % syns.size()];
		}
		auto timePerWord = 4.0f;
		bool correct = false;
		std::vector<Choice> choices = {
			{ targetWord, [&correct]() -> Task<> { correct = true; co_return; } },
			{ GetRandomWord(4, 12), []() -> Task<> { co_return; } },
			{ GetRandomWord(4, 12), []() -> Task<> { co_return; } },
			{ GetRandomWord(4, 12), []() -> Task<> { co_return; } },
		};
		std::shuffle(choices.begin(), choices.end(), m_mersenne); // Shuffle choices

		bool fastEnough = co_await Timeout(MultipleChoice(prompt, choices), timePerWord + 3.0f);
		if(fastEnough)
		{
			if(correct)
			{
				co_await Teletype("Very clever! Your brain has grown by one size!");
				++player.intelligence;
			}
			else
			{
				co_await Teletype("No, that's definitely wrong...");
			}
		}
		else
		{
			co_await Teletype("TOO SLOW! Training failed.");
		}
		if(!fastEnough || !correct)
		{
			co_await Teletype(std::string("The correct word was: ") + targetWord);
		}
		NewLine();
	}
	std::optional<Spell> GetRandomNewSpell(Character& player)
	{
		const auto& arc = m_spellArchive;
		const auto& sb = player.spellBook;
		std::vector<Spell> availableSpells;
		std::copy_if(arc.begin(), arc.end(), std::back_inserter(availableSpells), [&sb](const Spell& spell) {
			return sb.find(spell.shortcut) == sb.end();
		});
		if(availableSpells.size() == 0)
		{
			return {};
		}
		auto spell = availableSpells[RandInt() % availableSpells.size()];
		return spell;
	}
	std::optional<Spell> GetSpellByName(const std::string& in_name)
	{
		for(const auto& spell : m_spellArchive)
		{
			if(spell.name == in_name)
			{
				return spell;
			}
		}
		return {};
	}
	Task<> Practice_Spells(Character& player)
	{
		TASK_NAME(__FUNCTION__);

		// Early-out if we already know all the spell
		auto spellMaybe = GetRandomNewSpell(player);
		if(!spellMaybe)
		{
			co_await Teletype("The Sphinx has no more to teach you...", 2.0);
			NewLine();
			co_return;
		}

		--player.skillPoints;

		// Learn a new Spell (literally solve a riddle)
		co_await Teletype("The Great Sphinx stands before you!");
		co_await Teletype("She speaks: \"Answer me this riddle and I shall reveal what you seek...\"");
		NewLine();
		std::string riddle, answer;
		std::tie(riddle, answer) = m_data.riddles[RandInt() % m_data.riddles.size()];
		bool guessedCorrectly = false;
		auto guessesRemaining = 3;
		while(guessesRemaining > 0 && !guessedCorrectly)
		{
			co_await Teletype(riddle);
			std::stringstream guessesStr;
			guessesStr << "You have " << guessesRemaining << " guesses remaining...";
			co_await Teletype(guessesStr.str());

			auto input = co_await WaitForInput();
			auto spacePos = input.rfind(" ");
			if(spacePos != std::string::npos)
			{
				input = input.substr(spacePos + 1);
			}
			--guessesRemaining;
			if(!(guessedCorrectly = ToLower(input) == ToLower(answer)))
			{
				co_await Teletype("\"That is not the correct answer...\"");
				NewLine();
			}
		}

		// Respond to final guess
		if(guessedCorrectly)
		{
			co_await Teletype("\"Well done. You have proven yourself worthy...\"");
			NewLine();

			// Unlock the new spell for player
			auto spell = spellMaybe.value();
			player.LearnSpell(spell);
			co_await Teletype(std::string("Learned new spell: ") + spell.name + "!");
			std::stringstream spellStr;
			spellStr << spell.name << " (" << spell.mpCost << " MP) - " << spell.desc;
			co_await Teletype(spellStr.str());
			co_await Teletype(std::string("You can cast this spell during combat by pressing '") + spell.shortcut + "'!");
			NewLine();
		}
		else
		{
			co_await Teletype(std::string("You have failed... The true answer was '") + ToLower(answer) + "'", 2.0);
		}
	}

	// Combat
	Task<> Combat(Character& in_attacker, Character& in_defender)
	{
		TASK_NAME(__FUNCTION__);

		// Clear conditions as soon as attacker exist combat
		auto clearConditionsGuard = MakeFnGuard([&in_attacker] {
			in_attacker.ClearConditions();
		});

		while(in_attacker.health > 0 && in_defender.health > 0)
		{
			bool hasHaste = in_attacker.conditions.hasteTokens;
			auto hasteBonus = hasHaste ? 0.5f : 1.0f;
			auto attackDelay = in_attacker.baseAttackDelay - (in_attacker.speed * 0.04f);
			attackDelay *= 2.0f; // Slowing down combat in general
			attackDelay = attackDelay < 0.1f ? 0.1f : attackDelay;
			co_await WaitSeconds(attackDelay * hasteBonus + Rand() * 0.1f);
			bool isStunned = in_attacker.conditions.stunTokens;
			if(isStunned)
			{
				co_await WaitSeconds(2.0f);
			}
			float dmg = (float)in_attacker.strength;
			bool defHasFortify = in_defender.conditions.fortifyTokens;
			dmg -= in_defender.armor + (defHasFortify ? 2 : 0);
			auto defensePct = 1.0f - (in_defender.defense * 0.06f) - (defHasFortify ? 0.2f : 0.0f);
			defensePct = defensePct < 0.2f ? 0.2f : defensePct;
			dmg *= defensePct;
			int32_t finalDmg = (int32_t)dmg;
			finalDmg = finalDmg < 1 ? 1 : finalDmg;
			in_defender.health -= finalDmg;
			in_defender.health = in_defender.health < 0 ? 0 : in_defender.health;

			std::stringstream attackStr;
			attackStr << in_attacker.name << " hit " << in_defender.name << " for " << finalDmg << " damage! (" << in_defender.health << "/" << in_defender.maxHealth << "HP)";
			co_await Teletype(attackStr.str(), 0.0f, 0.0f);
		}
	}

	// Magic Combat
	Task<> PlayerMagic(Character& in_attacker, Character& in_defender)
	{
		TASK_NAME(__FUNCTION__);

		auto TryToCastSpell = [this, &in_attacker, &in_defender](const Spell& in_spell) -> Task<>
		{
			TASK_NAME("TryToCastSpell", [name = in_spell.name] { return name; });

			if(in_attacker.mana >= in_spell.mpCost)
			{
				// Spend mana
				in_attacker.mana -= in_spell.mpCost;
				in_attacker.mana = in_attacker.mana < 0 ? 0 : in_attacker.mana;

				// Run task function
				co_await in_spell.taskFn(in_spell, in_attacker, in_defender);
			}
			else
			{
				std::stringstream mpMsgStr;
				mpMsgStr << "*** Cannot cast " << in_spell.name << " - not enough MP! (" << in_attacker.mana << "/" << in_spell.mpCost << " MP)";
				co_await Teletype(mpMsgStr.str(), 0.0, 0.0);
			}
		};

		while(true)
		{
			// Listen for single-key input (non-blocking)
			auto c = co_await WaitForInputChar();

			// Lookup spell in spell book
			auto found = in_attacker.spellBook.find(c);
			if(found != in_attacker.spellBook.end())
			{
				// Try to cast spell
				const auto& spell = found->second;
				co_await TryToCastSpell(spell);

				// Wait for spell cooldown
				if(spell.cooldown > 0.0)
				{
					co_await Timeout([this, &in_attacker]() -> Task<> {
						TASK_NAME("Spell Cooldown");

						while(true)
						{
							auto c = co_await WaitForInputChar();
							if(in_attacker.spellBook.find(c) != in_attacker.spellBook.end())
							{
								co_await Teletype("You must wait before casting another spell", 0.0, 0.0);
							}
						}
					}(), spell.cooldown);

					continue; // Immediately wait for char input again
				}
			}
			co_await Suspend();
		}
	}

	// Spells
	void GenerateSpellArchive()
	{
#define BIND_SPELL(taskFn) std::bind(&TextGame::Spell_##taskFn, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
		m_spellArchive = {
			{ BIND_SPELL(Bolt),		'l', "Lightning Bolt", 5, 1.0, "Throw a lightning bolt, like Zeus" },
			{ BIND_SPELL(Heal),		'h', "Heal", 2, 1.0, "Restore some of your HP" },
			{ BIND_SPELL(Quicken),	'q', "Quicken", 5, 1.0, "Increases attack speed for 5 seconds" },
			{ BIND_SPELL(Regen),	'r', "Regeneration", 4, 1.0, "Heals HP periodically for a short time" },
			{ BIND_SPELL(Poison),	'p', "Poison", 4, 1.0, "Damages enemy periodically for a short time" },
			{ BIND_SPELL(Stun),		'p', "Stun", 5, 1.0, "Prevents enemy from attacking for a short time" },
			{ BIND_SPELL(Fortify),	'p', "Fortify", 5, 1.0, "Gain armor and defense for a short time" },
		};
#undef BIND_SPELL
	}
	Task<> Spell_Bolt(const Spell& in_spell, Character& in_attacker, Character& in_defender)
	{
		TASK_NAME(__FUNCTION__);

		auto boltDmg = Lookup(in_attacker.intelligence, std::vector<int32_t>{0, 1, 2, 3, 4, 6, 8, 10, 13, 16, 20});
		in_defender.health -= boltDmg; // Bolt damage

		std::stringstream attackStr;
		attackStr << "*** " << in_attacker.name << " casts " << in_spell.name << " at " << in_defender.name << " for " << in_spell.mpCost << " MP!\n";
		attackStr << "*** " << "Bolt hit for " << boltDmg << " damage! (" << in_defender.health << " / " << in_defender.maxHealth << "HP)";
		co_await Teletype(attackStr.str(), 0.0, 0.0);
	}
	Task<> Spell_Heal(const Spell& in_spell, Character& in_attacker, Character& in_defender)
	{
		TASK_NAME(__FUNCTION__);

		auto healAmount = Lookup(in_attacker.intelligence, std::vector<int32_t>{0, 1, 2, 3, 4, 6, 8, 10, 13, 16, 20});
		in_attacker.health += healAmount; // Heal attacker

		std::stringstream healStr;
		healStr << "*** " << in_attacker.name << " casts Heal for " << in_spell.mpCost << " MP!\n";
		healStr << "*** " << "Healed for " << healAmount << " HP! (" << in_attacker.health << " / " << in_attacker.maxHealth << "HP)";
		co_await Teletype(healStr.str(), 0.0, 0.0);
	}
	Task<> Spell_Quicken(const Spell& in_spell, Character& in_attacker, Character& in_defender)
	{
		TASK_NAME(__FUNCTION__);

		// Create persistent condition task that grants haste for N seconds
		in_attacker.conditions.conditionTasks.push_back(m_taskMgr.Run([](Character& in_attacker) -> Task<> {
			TASK_NAME("Quicken Condition");

			auto token = in_attacker.conditions.hasteTokens.TakeToken("Quicken Spell");
			float quickenDur = 5.0;
			co_await WaitSeconds(quickenDur);
		}(in_attacker))); // NOTE: Non-co_awaited lambda coroutines should always pass in their params, rather than capture them!

		std::stringstream quickenStr;
		quickenStr << "*** " << in_attacker.name << " casts Quicken for " << in_spell.mpCost << " MP!";
		co_await Teletype(quickenStr.str(), 0.0, 0.0);
	}
	Task<> Spell_Regen(const Spell& in_spell, Character& in_attacker, Character& in_defender)
	{
		TASK_NAME(__FUNCTION__);

		// Create persistent condition task that grants regen
		in_attacker.conditions.conditionTasks.push_back(m_taskMgr.Run([](TextGame* self, Character& in_attacker) -> Task<> {
			TASK_NAME("Regen Condition");

			auto token = in_attacker.conditions.regenTokens.TakeToken("Regen Spell");
			float regenDelayTime = 0.8f;
			int32_t totalRegens = Lookup(in_attacker.intelligence, std::vector<int32_t>{0, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6});

			while(totalRegens-- > 0)
			{
				co_await WaitSeconds(regenDelayTime);
				auto healAmount = Lookup(in_attacker.intelligence, std::vector<int32_t>{0, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3});
				in_attacker.health += healAmount; // Heal attacker
				in_attacker.health = in_attacker.health > in_attacker.maxHealth ? in_attacker.maxHealth : in_attacker.health;

				std::stringstream regenStr;
				regenStr << "*** " << "Regen spell healed " << in_attacker.name << " for " << healAmount << " HP! (" << in_attacker.health << " / " << in_attacker.maxHealth << "HP)";
				co_await self->Teletype(regenStr.str(), 0.0f, 0.0f);
			}
		}(this, in_attacker)));

		std::stringstream regenStr;
		regenStr << "*** " << in_attacker.name << " casts Regen for " << in_spell.mpCost << " MP!";
		co_await Teletype(regenStr.str(), 0.0, 0.0);
	}
	Task<> Spell_Poison(const Spell& in_spell, Character& in_attacker, Character& in_defender)
	{
		TASK_NAME(__FUNCTION__);

		// Create condition coroutine that afflicts poison for N seconds
		in_attacker.conditions.conditionTasks.push_back(m_taskMgr.Run([](TextGame* self, Character& in_attacker, Character& in_defender) -> Task<> {
			TASK_NAME("Poison Condition");

			auto token = in_defender.conditions.poisonTokens.TakeToken("Poison Spell");
			float poisonDelayTime = 1.2f;
			int32_t totalPoisons = Lookup(in_attacker.intelligence, std::vector<int32_t>{0, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6});

			while(totalPoisons-- > 0)
			{
				co_await WaitSeconds(poisonDelayTime);
				auto dmg = Lookup(in_attacker.intelligence, std::vector<int32_t>{0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2});
				in_defender.health -= dmg; // Heal attacker
				in_defender.health = in_defender.health < 0 ? 0 : in_defender.health;

				std::stringstream poisonStr;
				poisonStr << "*** " << "Poison spell damaged " << in_defender.name << " for " << dmg << " damage! (" << in_defender.health << " / " << in_defender.maxHealth << "HP)";
				co_await self->Teletype(poisonStr.str(), 0.0f, 0.0f);
			}
		}(this, in_attacker, in_defender)));

		std::stringstream posionStr;
		posionStr << "*** " << in_attacker.name << " casts Poison for " << in_spell.mpCost << " MP!";
		co_await Teletype(posionStr.str(), 0.0f, 0.0f);
	}
	Task<> Spell_Stun(const Spell& in_spell, Character& in_attacker, Character& in_defender)
	{
		TASK_NAME(__FUNCTION__);

		// Create persistent condition task that stuns enemy for N seconds
		in_attacker.conditions.conditionTasks.push_back(m_taskMgr.Run([](Character& in_attacker, Character& in_defender) -> Task<> {
			TASK_NAME("Stun Condition");

			auto token = in_defender.conditions.stunTokens.TakeToken("Stun Spell");
			float stunDur = (float)Lookup(in_attacker.intelligence, std::vector<int32_t>{0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2});
			co_await WaitSeconds(stunDur);
		}(in_attacker, in_defender))); // NOTE: Non-co_awaited lambda coroutines should always pass in their params, rather than capture them!

		std::stringstream stunStr;
		stunStr << "*** " << in_attacker.name << " casts Stun for " << in_spell.mpCost << " MP!";
		co_await Teletype(stunStr.str(), 0.0f, 0.0f);
	}
	Task<> Spell_Fortify(const Spell& in_spell, Character& in_attacker, Character& in_defender)
	{
		TASK_NAME(__FUNCTION__);

		// Create persistent condition task that fortifies enemy for 5 seconds
		in_attacker.conditions.conditionTasks.push_back(m_taskMgr.Run([](Character& in_attacker) -> Task<> {
			TASK_NAME("Fortify Condition");

			auto token = in_attacker.conditions.fortifyTokens.TakeToken("Fortify Spell");
			float fortifyDur = 5.0;
			co_await WaitSeconds(fortifyDur);
		}(in_attacker))); // NOTE: Non-co_awaited lambda coroutines should always pass in their params, rather than capture them!

		std::stringstream fortifyStr;
		fortifyStr << "*** " << in_attacker.name << " casts Fortify for " << in_spell.mpCost << " MP!";
		co_await Teletype(fortifyStr.str(), 0.0, 0.0);
	}

	// Enemies
	Character GetRandomEnemy(int32_t in_stage)
	{
		auto Def = [](float pct) {
			return (int32_t)((1.0f - pct) / 0.06);
		};
		Character gobling = {
			"Gobling", 5, 5, 0, 0,
			1, 1, 1,
			1, 0, 0, 0, 1.1f
		};
		Character fairy = {
			"Fairy", 7, 7, 0, 0,
			1, 1, 1,
			1, 0, Def(0.7f), 0, 0.4f
		};
		Character banshee = {
			"Banshee", 15, 15, 0, 0,
			1, 1, 1,
			10, 1, 0, 0, 2
		};
		Character willowisp = {
			"Will-O-Wisp", 6, 6, 0, 0,
			1, 1, 1,
			7, 0, Def(0.1f), 0, 0.25f
		};
		Character manticore = {
			"Manticore", 25, 25, 0, 0,
			1, 1, 1,
			15, 2, Def(0.85f), 0, 1.25f
		};
		Character behemoth = {
			"Behemoth", 45, 45, 0, 0,
			1, 1, 1,
			25, 2, 0, 0, 1.33f
		};
		Character wizard = {
			"Wizard", 17, 17, 0, 0,
			1, 1, 1,
			40, 12, Def(0.3f), 0, 0.6f
		};
		Character dragon = {
			"Dragon", 85, 85, 0, 0,
			1, 1, 1,
			65, 5, Def(0.4f), 0, 1.2f
		};

		// Select enemy by stage
		switch(in_stage)
		{
		case 1: return gobling;
		case 2: return fairy;
		case 3: return banshee;
		case 4: return willowisp;
		case 5: return manticore;
		case 6: return behemoth;
		case 7: return wizard;
		case 8: return dragon;
		default: break;
		}

		return {
			"Thanatos the Undying", 85 + in_stage * 5, 85 + in_stage * 5, 0, 0,
			1, 1, 1,
			65 + in_stage * 12, (int32_t)(5 + in_stage * 0.2f), Def(0.4f), 0, 1.2f / (1 + (in_stage - 8) / 15)
		};
	}

	// Misc Tasks
	Task<std::string> WaitForInput()
	{
		return m_textInput.WaitForInput();
	}
	Task<char> WaitForInputChar()
	{
		return m_textInput.WaitForInputChar();
	}
	Task<> ConfirmQuit()
	{
		TASK_NAME(__FUNCTION__);

		co_await TeletypeChoice("Are you sure? (Y/N)", 0.0);
		auto confirm = co_await WaitForInput();
		if(ToLower(confirm) == "y" || ToLower(confirm) == "yes")
		{
			m_isGameOver = true;
		}
	}

	// Multiple-Choice
	struct Choice
	{
		std::string name;
		std::function<Task<>()> taskFn;
	};
	Task<> MultipleChoice(const std::string& in_prompt, const std::vector<Choice>& in_choices)
	{
		TASK_NAME(__FUNCTION__);

		co_await Teletype(in_prompt);
		for(size_t i = 0; i < in_choices.size(); ++i)
		{
			const auto& choice = in_choices[i];
			std::stringstream choiceStr;
			choiceStr << (i + 1) << ") " << choice.name;
			co_await TeletypeChoice(choiceStr.str(), i == in_choices.size() - 1 ? 0.0f : 0.02f);
		}
		auto mode = co_await WaitForInput();
		for(size_t i = 0; i < in_choices.size(); ++i)
		{
			const auto& choice = in_choices[i];
			int32_t idx = StrToInt(mode).value_or(-1);
			if(idx == (i + 1) || ToLower(mode) == ToLower(choice.name))
			{
				NewLine();
				co_await choice.taskFn();
			}
		}
	}

	// Random helpers
	float Rand()
	{
		return m_randFloat(m_mersenne);
	}
	int32_t RandInt()
	{
		return (int32_t)(Rand() * std::numeric_limits<int32_t>::max());
	}
	int32_t RandInRange(int32_t in_min, int32_t in_max)
	{
		int32_t ret = (RandInt() % (in_max - in_min + 1)) + in_min;
		return ret;
	}

	// Data helpers
	template <typename T>
	static T Lookup(int32_t in_key, const std::vector<T>& in_vals)
	{
		SQUID_RUNTIME_CHECK(in_vals.size(), "Attempted to lookup into an empty value set");

		if(in_key < 0)
		{
			return in_vals[0];
		}
		else if(in_key >= (int32_t)in_vals.size())
		{
			return in_vals.back();
		}
		else
		{
			return in_vals[in_key];
		}
	}

	// String helpers
	static std::vector<std::string> Split(const std::string in_str, const std::string in_delim)
	{
		std::vector<std::string> tokens;
		size_t start = 0;
		size_t end = in_str.find(in_delim);
		while(end != std::string::npos)
		{
			auto token = in_str.substr(start, end - start);
			if(token.size() > 0)
			{
				tokens.push_back(token);
			}
			start = end + in_delim.size();
			end = in_str.find(in_delim, start);
		}
		auto token = in_str.substr(start, end - start);
		if(token.size() > 0)
		{
			tokens.push_back(token);
		}
		return tokens;
	}

	// Text helpers
	void NewLine()
	{
		std::cout << std::endl;
	}
	std::string ToLower(std::string in_str)
	{
		std::transform(in_str.begin(), in_str.end(), in_str.begin(), [](unsigned char c) { return std::tolower(c); });
		return in_str;
	}
	bool IsStrNumeric(const std::string& in_str)
	{
		return !in_str.empty() && std::find_if(in_str.begin(), in_str.end(), [](unsigned char c) { return !std::isdigit(c); }) == in_str.end();
	}
	std::optional<int32_t> StrToInt(const std::string& in_str)
	{
		if(IsStrNumeric(in_str))
		{
			int32_t i = std::stoi(in_str.c_str());
			return i;
		}
		else
		{
			return {};
		}
	}
	Task<> Teletype(const std::string& in_str, float in_delay = 0.5, float in_rate = 0.03)
	{
		TASK_NAME(__FUNCTION__);

		for(auto c : in_str)
		{
			std::cout << c;
			co_await WaitSeconds(in_rate);
		}
		co_await WaitSeconds(in_delay);
		NewLine();
	}
	Task<> TeletypeChoice(const std::string& in_str, float in_delay = 0.25, float in_rate = 0.02)
	{
		return Teletype(in_str, in_delay, in_rate);
	}

	// Simple spoiler encryption
	static std::string Rot13(std::string in_str)
	{
		const int LOWER_A = 97;
		const int LOWER_M = 109;
		const int LOWER_N = 110;
		const int LOWER_Z = 122;

		const int UPPER_A = 65;
		const int UPPER_M = 77;
		const int UPPER_N = 78;
		const int UPPER_Z = 90;

		for(size_t index = 0; index < in_str.size(); ++index)
		{
			auto c = in_str[index];
			if(c >= LOWER_A && c <= LOWER_M)
				in_str[index] = c + 13;
			else if(c >= LOWER_N && c <= LOWER_Z)
				in_str[index] = c - 13;
			else if(c >= UPPER_A && c <= UPPER_M)
				in_str[index] = c + 13;
			else if(c >= UPPER_N && c <= UPPER_Z)
				in_str[index] = c - 13;
		}
		return in_str;
	}
};
