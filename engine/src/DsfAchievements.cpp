#include "global.h"
#include "DsfAchievements.h"
#include "IniFile.h"
#include "RageLog.h"
#include "RageUtil.h"

#include <ctime>
#include <map>
#include <set>

namespace
{
	const CString PATH = "Data/Achievements.ini";
	const int SURVIVOR_ID = 0x8038, HEAVEN_ID = 0x803B;
	/* The songs with the PC version's own gameplay backgrounds: Tough Enough,
	 * Go West, Kids in America, Heaven and Earth (new songs) and Mickey. */
	const int SCENERY_IDS[] = { 0x8030, 0x8034, 0x8037, 0x803B, 0x800F };
	const int NUM_SCENERY = ARRAYSIZE( SCENERY_IDS );
	const int GRADE_AA = 3, GRADE_A = 4;	/* the game's grade codes */
	const int EXPERT = 3;

	enum Id
	{
		FIRST_STEPS, STANDARD, DIFFICULT, EXPERT_FEET, FULL_COMBO, EXPERT_FULL_COMBO, ALL_PERFECT,
		COMPLETIONIST, EXPERT_COMPLETIONIST, DOUBLE_A, STRAIGHT_AS, COMBO_100, COMBO_300, COMBO_500,
		SURVIVOR, HEAVEN, SOMETHING_NEW, ALL_NEW, SCENERY, DOUBLE_TROUBLE, WORKOUT, LESSON,
		REGULAR, MARATHON, CALORIES, NUM_ACHIEVEMENTS
	};
	struct Def { const char *szKey, *szName, *szDescription; };
	const Def DEFS[NUM_ACHIEVEMENTS] =
	{
		{ "FirstSteps", "First Steps", "Clear a song." },
		{ "Standard", "Standard Issue", "Clear a song on STANDARD." },
		{ "Difficult", "Getting Serious", "Clear a song on DIFFICULT." },
		{ "Expert", "Expert Feet", "Clear a song on EXPERT." },
		{ "FullCombo", "Full Combo", "Clear a song without a single GOOD, ALMOST or BOO." },
		{ "ExpertFullCombo", "Flawless Expert", "Full combo a song on EXPERT." },
		{ "AllPerfect", "Nothing but PERFECT", "Clear a song with every step PERFECT." },
		{ "Completionist", "Completionist", "Clear every song in the music select." },
		{ "ExpertCompletionist", "Expert Completionist", "Clear every song in the music select on EXPERT." },
		{ "DoubleA", "Double A", "Get an AA." },
		{ "StraightAs", "Straight A's", "Get an A or better on 10 different songs." },
		{ "Combo100", "Combo 100", "Reach a 100 combo." },
		{ "Combo300", "Combo 300", "Reach a 300 combo." },
		{ "Combo500", "Combo 500", "Reach a 500 combo." },
		{ "Survivor", "Survivor", "Clear PARANOIA survivor on DIFFICULT or EXPERT." },
		{ "HeavenAndEarth", "Heaven and Earth", "Play Heaven and Earth." },
		{ "SomethingNew", "Something New", "Play one of the new songs." },
		{ "AllNew", "All New", "Play every one of the new songs." },
		{ "Scenery", "Change of Scenery", "Play the 5 songs with the PC version's own backgrounds: Tough Enough, Go West, Kids in America, Heaven and Earth and Mickey." },
		{ "DoubleTrouble", "Double Trouble", "Clear a song in DOUBLE." },
		{ "Workout", "Feel the Burn", "Finish a song in WORKOUT MODE." },
		{ "Lesson", "Back to School", "Finish a section of LESSON MODE." },
		{ "Regular", "Regular", "Play 100 songs." },
		{ "Marathon", "Marathon", "Hit 10,000 steps (PERFECT, GREAT or GOOD)." },
		{ "Calories", "Calorie Counter", "Burn 1,000 kcal in WORKOUT MODE." },
	};

	/* What counts towards them; all of it is kept, so a new memory card
	 * takes nothing away. */
	struct State
	{
		bool bLoaded;
		std::map<CString, CString> Earned;	/* key -> date */
		std::set<int> Cleared, ClearedExpert, AOrBetter, PlayedNew, PlayedScenery;
		bool aClearedChart[4];
		bool bFullCombo, bExpertFullCombo, bAllPerfect, bAA, bSurvivor, bHeaven, bDouble, bWorkout, bLesson;
		int iBestCombo, iStepsHit, iKcal;
		int iSongsPlayed;			/* songs finished since the PC kept count */
		int iSavePlays;				/* cleared plays on the memory card (all time) */
		int iListSongs, iNewSongs;		/* from the last save report */
	};
	State g;
	vector<CString> g_Pending;

	CString IdsToString( const std::set<int> &s )
	{
		CString sOut;
		for( std::set<int>::const_iterator it = s.begin(); it != s.end(); ++it )
			sOut += ssprintf( "%s%x", sOut.empty()? "": ",", *it );
		return sOut;
	}

	void IdsFromString( const CString &s, std::set<int> &out )
	{
		CStringArray as;
		split( s, ",", as );
		for( unsigned i = 0; i < as.size(); ++i )
			out.insert( (int) strtol(as[i], NULL, 16) );
	}

	void Load()
	{
		if( g.bLoaded )
			return;
		g.bLoaded = true;
		for( int c = 0; c < 4; ++c )
			g.aClearedChart[c] = false;
		g.bFullCombo = g.bExpertFullCombo = g.bAllPerfect = g.bAA = g.bSurvivor = g.bHeaven = false;
		g.bDouble = g.bWorkout = g.bLesson = false;
		g.iBestCombo = g.iSongsPlayed = g.iSavePlays = g.iStepsHit = g.iKcal = 0;
		g.iListSongs = 32;
		g.iNewSongs = 12;

		IniFile ini;
		if( !ini.ReadFile(PATH) )
			return;
		for( int i = 0; i < NUM_ACHIEVEMENTS; ++i )
		{
			CString sDate;
			if( ini.GetValue("Earned", DEFS[i].szKey, sDate) )
				g.Earned[DEFS[i].szKey] = sDate;
		}
		CString s;
		if( ini.GetValue("Songs", "Cleared", s) ) IdsFromString( s, g.Cleared );
		if( ini.GetValue("Songs", "ClearedExpert", s) ) IdsFromString( s, g.ClearedExpert );
		if( ini.GetValue("Songs", "AOrBetter", s) ) IdsFromString( s, g.AOrBetter );
		if( ini.GetValue("Songs", "PlayedNew", s) ) IdsFromString( s, g.PlayedNew );
		if( ini.GetValue("Songs", "PlayedScenery", s) ) IdsFromString( s, g.PlayedScenery );
		for( int c = 0; c < 4; ++c )
			ini.GetValue( "Done", ssprintf("ClearedChart%d", c), g.aClearedChart[c] );
		ini.GetValue( "Done", "FullCombo", g.bFullCombo );
		ini.GetValue( "Done", "ExpertFullCombo", g.bExpertFullCombo );
		ini.GetValue( "Done", "AllPerfect", g.bAllPerfect );
		ini.GetValue( "Done", "AA", g.bAA );
		ini.GetValue( "Done", "Survivor", g.bSurvivor );
		ini.GetValue( "Done", "Heaven", g.bHeaven );
		ini.GetValue( "Done", "Double", g.bDouble );
		ini.GetValue( "Done", "Workout", g.bWorkout );
		ini.GetValue( "Done", "Lesson", g.bLesson );
		ini.GetValue( "Counts", "BestCombo", g.iBestCombo );
		ini.GetValue( "Counts", "SongsPlayed", g.iSongsPlayed );
		ini.GetValue( "Counts", "SavePlays", g.iSavePlays );
		ini.GetValue( "Counts", "StepsHit", g.iStepsHit );
		ini.GetValue( "Counts", "Kcal", g.iKcal );
		ini.GetValue( "Counts", "ListSongs", g.iListSongs );
		ini.GetValue( "Counts", "NewSongs", g.iNewSongs );
	}

	void Save()
	{
		IniFile ini;
		for( std::map<CString, CString>::const_iterator it = g.Earned.begin(); it != g.Earned.end(); ++it )
			ini.SetValue( "Earned", it->first, it->second );
		ini.SetValue( "Songs", "Cleared", IdsToString(g.Cleared) );
		ini.SetValue( "Songs", "ClearedExpert", IdsToString(g.ClearedExpert) );
		ini.SetValue( "Songs", "AOrBetter", IdsToString(g.AOrBetter) );
		ini.SetValue( "Songs", "PlayedNew", IdsToString(g.PlayedNew) );
		ini.SetValue( "Songs", "PlayedScenery", IdsToString(g.PlayedScenery) );
		for( int c = 0; c < 4; ++c )
			ini.SetValue( "Done", ssprintf("ClearedChart%d", c), g.aClearedChart[c] );
		ini.SetValue( "Done", "FullCombo", g.bFullCombo );
		ini.SetValue( "Done", "ExpertFullCombo", g.bExpertFullCombo );
		ini.SetValue( "Done", "AllPerfect", g.bAllPerfect );
		ini.SetValue( "Done", "AA", g.bAA );
		ini.SetValue( "Done", "Survivor", g.bSurvivor );
		ini.SetValue( "Done", "Heaven", g.bHeaven );
		ini.SetValue( "Done", "Double", g.bDouble );
		ini.SetValue( "Done", "Workout", g.bWorkout );
		ini.SetValue( "Done", "Lesson", g.bLesson );
		ini.SetValue( "Counts", "BestCombo", g.iBestCombo );
		ini.SetValue( "Counts", "SongsPlayed", g.iSongsPlayed );
		ini.SetValue( "Counts", "SavePlays", g.iSavePlays );
		ini.SetValue( "Counts", "StepsHit", g.iStepsHit );
		ini.SetValue( "Counts", "Kcal", g.iKcal );
		ini.SetValue( "Counts", "ListSongs", g.iListSongs );
		ini.SetValue( "Counts", "NewSongs", g.iNewSongs );
		if( !ini.WriteFile(PATH) )
			LOG->Warn( "DsfAchievements: can't write %s", PATH.c_str() );
	}

	bool IsScenery( int id )
	{
		for( int i = 0; i < NUM_SCENERY; ++i )
			if( SCENERY_IDS[i] == id )
				return true;
		return false;
	}

	/* How far along achievement i is.  iTarget 0: done or not, no count. */
	void Measure( int i, int &iProgress, int &iTarget, bool &bDone )
	{
		iProgress = 0;
		iTarget = 0;
		bDone = false;
		switch( i )
		{
		case FIRST_STEPS:	bDone = !g.Cleared.empty(); break;
		case STANDARD:		bDone = g.aClearedChart[1]; break;
		case DIFFICULT:		bDone = g.aClearedChart[2]; break;
		case EXPERT_FEET:	bDone = g.aClearedChart[3]; break;
		case FULL_COMBO:	bDone = g.bFullCombo; break;
		case EXPERT_FULL_COMBO:	bDone = g.bExpertFullCombo; break;
		case ALL_PERFECT:	bDone = g.bAllPerfect; break;
		case COMPLETIONIST:	iProgress = (int) g.Cleared.size(); iTarget = g.iListSongs; break;
		case EXPERT_COMPLETIONIST: iProgress = (int) g.ClearedExpert.size(); iTarget = g.iListSongs; break;
		case DOUBLE_A:		bDone = g.bAA; break;
		case STRAIGHT_AS:	iProgress = (int) g.AOrBetter.size(); iTarget = 10; break;
		case COMBO_100:		iProgress = g.iBestCombo; iTarget = 100; break;
		case COMBO_300:		iProgress = g.iBestCombo; iTarget = 300; break;
		case COMBO_500:		iProgress = g.iBestCombo; iTarget = 500; break;
		case SURVIVOR:		bDone = g.bSurvivor; break;
		case HEAVEN:		bDone = g.bHeaven; break;
		case SOMETHING_NEW:	bDone = !g.PlayedNew.empty(); break;
		case ALL_NEW:		iProgress = (int) g.PlayedNew.size(); iTarget = g.iNewSongs; break;
		case SCENERY:		iProgress = (int) g.PlayedScenery.size(); iTarget = NUM_SCENERY; break;
		case DOUBLE_TROUBLE:	bDone = g.bDouble; break;
		case WORKOUT:		bDone = g.bWorkout; break;
		case LESSON:		bDone = g.bLesson; break;
		/* the card counts cleared plays, the PC every finished song since the
		 * update; each counts a play the other may have too, so the larger */
		case REGULAR:		iProgress = max( g.iSongsPlayed, g.iSavePlays ); iTarget = 100; break;
		case MARATHON:		iProgress = g.iStepsHit; iTarget = 10000; break;
		case CALORIES:		iProgress = g.iKcal / 1000; iTarget = 1000; break;
		}
		if( iTarget > 0 )
		{
			bDone = iProgress >= iTarget;
			iProgress = min( iProgress, iTarget );
		}
	}

	CString Today()
	{
		time_t t = time( NULL );
		char sz[16];
		strftime( sz, sizeof(sz), "%Y-%m-%d", localtime(&t) );
		return sz;
	}

	/* After a change: mark what is newly done, and keep it all. */
	void Update()
	{
		for( int i = 0; i < NUM_ACHIEVEMENTS; ++i )
		{
			int iProgress, iTarget;
			bool bDone;
			Measure( i, iProgress, iTarget, bDone );
			if( !bDone || g.Earned.find(DEFS[i].szKey) != g.Earned.end() )
				continue;
			g.Earned[DEFS[i].szKey] = Today();
			g_Pending.push_back( DEFS[i].szName );
			LOG->Trace( "DsfAchievements: earned %s", DEFS[i].szName );
		}
		Save();
	}

	void Cleared( int id, int iChart, bool bDouble, int iGrade )
	{
		g.Cleared.insert( id );
		g.aClearedChart[iChart] = true;
		if( iChart == EXPERT )
			g.ClearedExpert.insert( id );
		if( bDouble )
			g.bDouble = true;
		if( iGrade >= 1 && iGrade <= GRADE_AA )
			g.bAA = true;
		if( iGrade >= 1 && iGrade <= GRADE_A )
			g.AOrBetter.insert( id );
		if( id == SURVIVOR_ID && iChart >= 2 )
			g.bSurvivor = true;
	}

	void Played( int id, bool bNewSong )
	{
		if( bNewSong )
			g.PlayedNew.insert( id );
		if( IsScenery(id) )
			g.PlayedScenery.insert( id );
		if( id == HEAVEN_ID )
			g.bHeaven = true;
	}
}

void DsfAchievements::OnPlay( const DsfCorePlay &p )
{
	Load();
	LOG->Trace( "DsfAchievements: player %d song %04x chart %d%s%s cleared %d grade %d judgments %d %d %d %d %d combo %d/%d kcal %.3f",
		p.player, p.song_id, p.chart, p.double_style? " double": "", p.workout? " workout": "", p.cleared, p.grade,
		p.perfect, p.great, p.good, p.almost, p.boo, p.max_combo, p.steps, p.kcal_milli / 1000.0f );
	if( p.song_id == 0 || p.chart < 0 || p.chart > 3 )
		return;

	++g.iSongsPlayed;
	g.iStepsHit += p.perfect + p.great + p.good;
	g.iBestCombo = max( g.iBestCombo, p.max_combo );
	Played( p.song_id, p.new_song != 0 );
	if( p.workout )
		g.bWorkout = true;
	if( p.cleared )
	{
		Cleared( p.song_id, p.chart, p.double_style != 0, p.grade );
		const bool bFullCombo = p.steps > 0 && p.good == 0 && p.almost == 0 && p.boo == 0;
		if( bFullCombo )
		{
			g.bFullCombo = true;
			if( p.chart == EXPERT )
				g.bExpertFullCombo = true;
			if( p.great == 0 )
				g.bAllPerfect = true;
		}
	}
	Update();
}

void DsfAchievements::OnProgress( const DsfCoreProgress &pr )
{
	Load();
	int iPlays = 0, iSongs = 0;
	for( int i = 0; i < DSFCORE_SAVE_SONGS; ++i )
	{
		const DsfCoreSaveSong &s = pr.song[i];
		if( s.song_id == 0 )
			continue;
		++iSongs;
		iPlays += s.plays;
		g.iBestCombo = max( g.iBestCombo, (int) s.best_combo );
		bool bAny = false;
		for( int d = 0; d < 8; ++d )
		{
			if( s.grade[d] == 0 )
				continue;
			bAny = true;
			Cleared( s.song_id, d & 3, d >= 4, s.grade[d] );
			if( s.full_combo & (1 << d) )
			{
				g.bFullCombo = true;
				if( (d & 3) == EXPERT )
					g.bExpertFullCombo = true;
			}
		}
		if( bAny || s.plays > 0 )
			Played( s.song_id, s.new_song != 0 );
	}
	LOG->Trace( "DsfAchievements: save: %d songs, %d plays, %.3f kcal, lessons %x, list %d (%d new)",
		iSongs, iPlays, pr.workout_kcal_milli / 1000.0f, pr.lesson_sections, pr.list_songs, pr.new_songs );
	g.iSavePlays = max( g.iSavePlays, iPlays );
	g.iKcal = max( g.iKcal, (int) pr.workout_kcal_milli );
	if( pr.workout_kcal_milli > 0 )
		g.bWorkout = true;
	if( pr.lesson_sections != 0 )
		g.bLesson = true;
	if( pr.list_songs > 0 )
		g.iListSongs = pr.list_songs;
	if( pr.new_songs > 0 )
		g.iNewSongs = pr.new_songs;
	Update();
}

int DsfAchievements::GetCount()
{
	return NUM_ACHIEVEMENTS;
}

int DsfAchievements::GetEarnedCount()
{
	Load();
	int n = 0;
	for( int i = 0; i < NUM_ACHIEVEMENTS; ++i )
		n += g.Earned.find( DEFS[i].szKey ) != g.Earned.end();
	return n;
}

void DsfAchievements::GetInfo( int i, Info &out )
{
	Load();
	ASSERT( i >= 0 && i < NUM_ACHIEVEMENTS );
	out.sName = DEFS[i].szName;
	out.sDescription = DEFS[i].szDescription;
	std::map<CString, CString>::const_iterator it = g.Earned.find( DEFS[i].szKey );
	out.bEarned = it != g.Earned.end();
	out.sDate = out.bEarned? it->second: CString();
	bool bDone;
	Measure( i, out.iProgress, out.iTarget, bDone );
}

bool DsfAchievements::TakeEarned( vector<CString> &asNames )
{
	asNames = g_Pending;
	g_Pending.clear();
	return !asNames.empty();
}
