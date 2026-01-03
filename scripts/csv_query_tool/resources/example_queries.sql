-- =============================================================================
-- CSV Query Tool - Example Queries for Science Battle Simulator
-- =============================================================================
-- These queries are designed for simulation_stats.csv
-- Columns: unit_id, name, faction, points, quality, defense, models,
--          matches, wins, losses, draws, win_rate, game_win_rate
-- =============================================================================


-- -----------------------------------------------------------------------------
-- BASIC QUERIES
-- -----------------------------------------------------------------------------

-- View all data (limited to 100 rows)
SELECT * FROM simulation_stats LIMIT 100;

-- Get column list and types
DESCRIBE simulation_stats;

-- Count total rows
SELECT COUNT(*) as total_units FROM simulation_stats;

-- Distinct factions
SELECT DISTINCT faction FROM simulation_stats ORDER BY faction;


-- -----------------------------------------------------------------------------
-- TOP PERFORMERS
-- -----------------------------------------------------------------------------

-- Top 20 units by win rate (with minimum matches)
SELECT name, faction, matches, win_rate, points
FROM simulation_stats
WHERE matches >= 100
ORDER BY win_rate DESC
LIMIT 20;

-- Top units by total wins
SELECT name, faction, points, matches, wins, win_rate
FROM simulation_stats
WHERE matches >= 100
ORDER BY wins DESC
LIMIT 25;

-- Best win rates among affordable units
SELECT name, faction, points, win_rate, matches
FROM simulation_stats
WHERE matches >= 100 AND points < 200
ORDER BY win_rate DESC
LIMIT 25;


-- -----------------------------------------------------------------------------
-- FACTION ANALYSIS
-- -----------------------------------------------------------------------------

-- Faction performance summary
SELECT
    faction,
    COUNT(*) as unit_count,
    ROUND(AVG(win_rate), 4) as avg_win_rate,
    ROUND(AVG(points), 0) as avg_points,
    SUM(matches) as total_matches
FROM simulation_stats
GROUP BY faction
ORDER BY avg_win_rate DESC;

-- Best unit per faction
WITH ranked AS (
    SELECT
        name, faction, win_rate, matches,
        ROW_NUMBER() OVER (PARTITION BY faction ORDER BY win_rate DESC) as rank
    FROM simulation_stats
    WHERE matches >= 50
)
SELECT name, faction, win_rate, matches
FROM ranked
WHERE rank = 1
ORDER BY win_rate DESC;

-- Faction diversity (how many viable units)
SELECT
    faction,
    COUNT(*) as total_units,
    COUNT(CASE WHEN win_rate > 0.5 THEN 1 END) as above_avg_units,
    ROUND(COUNT(CASE WHEN win_rate > 0.5 THEN 1 END) * 100.0 / COUNT(*), 1) as viable_pct
FROM simulation_stats
WHERE matches >= 50
GROUP BY faction
ORDER BY viable_pct DESC;


-- -----------------------------------------------------------------------------
-- POINTS EFFICIENCY
-- -----------------------------------------------------------------------------

-- Best value units (win rate per point)
SELECT
    name, faction, points, win_rate,
    ROUND(win_rate / points * 100, 4) as efficiency_per_point
FROM simulation_stats
WHERE matches >= 50 AND points > 0
ORDER BY efficiency_per_point DESC
LIMIT 30;

-- Price brackets analysis
SELECT
    CASE
        WHEN points < 100 THEN 'Cheap (<100)'
        WHEN points < 200 THEN 'Medium (100-199)'
        WHEN points < 300 THEN 'Expensive (200-299)'
        ELSE 'Premium (300+)'
    END as price_bracket,
    COUNT(*) as unit_count,
    ROUND(AVG(win_rate), 4) as avg_win_rate,
    ROUND(AVG(game_win_rate), 4) as avg_game_win_rate
FROM simulation_stats
WHERE matches >= 50
GROUP BY price_bracket
ORDER BY MIN(points);


-- -----------------------------------------------------------------------------
-- STAT CORRELATIONS
-- -----------------------------------------------------------------------------

-- Defense vs win rate
SELECT
    defense,
    COUNT(*) as unit_count,
    ROUND(AVG(win_rate), 4) as avg_win_rate,
    ROUND(AVG(points), 0) as avg_points
FROM simulation_stats
WHERE matches >= 50
GROUP BY defense
ORDER BY defense;

-- Quality vs win rate
SELECT
    quality,
    COUNT(*) as unit_count,
    ROUND(AVG(win_rate), 4) as avg_win_rate,
    ROUND(AVG(points), 0) as avg_points
FROM simulation_stats
WHERE matches >= 50
GROUP BY quality
ORDER BY quality;

-- Model count vs win rate
SELECT
    models,
    COUNT(*) as unit_count,
    ROUND(AVG(win_rate), 4) as avg_win_rate,
    ROUND(AVG(points), 0) as avg_points
FROM simulation_stats
WHERE matches >= 50
GROUP BY models
ORDER BY models;


-- -----------------------------------------------------------------------------
-- ADVANCED: PERCENTILE ANALYSIS
-- -----------------------------------------------------------------------------

-- Win rate percentiles
SELECT
    PERCENTILE_CONT(0.25) WITHIN GROUP (ORDER BY win_rate) as p25,
    PERCENTILE_CONT(0.50) WITHIN GROUP (ORDER BY win_rate) as median,
    PERCENTILE_CONT(0.75) WITHIN GROUP (ORDER BY win_rate) as p75,
    PERCENTILE_CONT(0.90) WITHIN GROUP (ORDER BY win_rate) as p90
FROM simulation_stats
WHERE matches >= 50;

-- Units in top 10% by win rate
WITH stats AS (
    SELECT PERCENTILE_CONT(0.90) WITHIN GROUP (ORDER BY win_rate) as p90
    FROM simulation_stats WHERE matches >= 50
)
SELECT s.name, s.faction, s.win_rate, s.matches
FROM simulation_stats s, stats
WHERE s.matches >= 50 AND s.win_rate >= stats.p90
ORDER BY s.win_rate DESC;


-- -----------------------------------------------------------------------------
-- SEARCH QUERIES
-- -----------------------------------------------------------------------------

-- Find units by name (case-insensitive)
SELECT name, faction, points, win_rate
FROM simulation_stats
WHERE LOWER(name) LIKE '%warrior%'
ORDER BY win_rate DESC;

-- Find units in a points range
SELECT name, faction, points, win_rate, matches
FROM simulation_stats
WHERE points BETWEEN 150 AND 200
  AND matches >= 50
ORDER BY win_rate DESC;

-- Find units with specific quality/defense combo
SELECT name, faction, points, quality, defense, win_rate
FROM simulation_stats
WHERE quality = 3 AND defense = 3
  AND matches >= 50
ORDER BY win_rate DESC;


-- -----------------------------------------------------------------------------
-- COMPARATIVE ANALYSIS
-- -----------------------------------------------------------------------------

-- Compare game win rate vs unit win rate
SELECT name, faction, points,
       win_rate as unit_win_rate,
       game_win_rate,
       ROUND(game_win_rate - win_rate, 4) as game_boost
FROM simulation_stats
WHERE matches >= 100
ORDER BY game_boost DESC
LIMIT 20;

-- Units that draw frequently
SELECT name, faction, points, matches, draws,
       ROUND(draws * 100.0 / matches, 2) as draw_rate
FROM simulation_stats
WHERE matches >= 100
ORDER BY draw_rate DESC
LIMIT 20;
