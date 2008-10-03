----------------------------------------------------------------
--  QUEST ASSIGNMENT
----------------------------------------------------------------
--
--  Oblige Level Maker (C) 2006-2008 Andrew Apted
--
--  This program is free software; you can redistribute it and/or
--  modify it under the terms of the GNU General Public License
--  as published by the Free Software Foundation; either version 2
--  of the License, or (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU General Public License for more details.
--
----------------------------------------------------------------

--[[ *** CLASS INFORMATION ***

class ARENA
{
  -- an Arena is a group of rooms, generally with a locked door
  -- to a different arena (requiring the player to find the key
  -- or switch).  There is a start room and a target room.

  rooms : array(ROOM)  -- all the rooms in this arena

  conns : array(CONN)  -- all the direct connections between rooms
                       -- in this arena.  Note that teleporters always
                       -- go between rooms in the same arena

  start : ROOM  -- room which player enters this arena
                -- (map's start room for the very first arena)
                -- Never nil.

  target : ROOM -- room containing the key/switch to exit this
                -- arena, _OR_ the level's exit room itself.
                -- Never nil.

  lock : LOCK   -- what kind of key/switch will be in the 'target'
                -- room, or the string "EXIT" if this arena leads
                -- to the exit room.
                -- Never nil
  
  path : array(CONN)  -- full path of rooms from 'start' to 'target'
                      -- NOTE: may contain teleporters.

}


class LOCK
{
  conn : CONN   -- connection between two rooms (and two arenas)
                -- which is locked (keyed door, lowering bars, etc)

  kind : keyword  -- "KEY" or "SWITCH"

  item : string   -- denotes specific kind of key or switch

  -- used while decided what locks to add:

  branch_mode : keyword  -- "ON" or "OFF"


}


--------------------------------------------------------------]]

require 'defs'
require 'util'


function Quest_decide_start_room(arena)

  local function eval_room(R)
    -- small preference for indoor rooms
    local cost = sel(R.outdoor, 0, 15)

    -- preference for leaf rooms
    cost = cost + 40 * ((#R.conns) ^ 0.6)

    -- large amount of randomness
    cost = cost + 100 * (1 - gui.random() * gui.random())

    return cost
  end

  local function swap_conn(C)
    C.src, C.dest = C.dest, C.src
    C.src_S, C.dest_S = C.dest_S, C.src_S
  end

  local function natural_flow(R, visited)
    visited[R] = true

    for _,C in ipairs(R.conns) do
      if R == C.dest and not visited[C.src] then
        swap_conn(C)
      end
      if R == C.src and not visited[C.dest] then
        natural_flow(C.dest, visited)
      end
    end

    for _,T in ipairs(R.teleports) do
      if R == T.dest and not visited[T.src] then
        swap_conn(T)
      end
      if R == T.src and not visited[T.dest] then
        natural_flow(T.dest, visited)
      end
    end
  end


  ---| Quest_decide_start_room |---

  for _,R in ipairs(arena.rooms) do
    R.start_cost = eval_room(R)
    gui.debugf("Room L(%d,%d) : START COST : %1.4f\n", R.lx1,R.ly1, R.start_cost)
  end

  arena.start = table_sorted_first(arena.rooms, function(A,B) return A.start_cost < B.start_cost end)

  gui.printf("Start room S(%d,%d)\n", arena.start.sx1, arena.start.sy1)

  -- update connections so that 'src' and 'dest' follow the natural
  -- flow of the level, i.e. player always walks src -> dest (except
  -- when backtracking).
  natural_flow(arena.start, {})
end


function Quest_update_tvols(arena)

  function travel_volume(R, seen_conns)
    -- Determine total volume of rooms that are reachable from the
    -- given room R, including itself, but excluding connections
    -- that have been "locked" or already seen.

    local total = assert(R.svolume)

    for _,C in ipairs(R.conns) do
      if not C.lock and not seen_conns[C] then
        local N = C:neighbor(R)
        seen_conns[C] = true
        total = total + travel_volume(N, seen_conns)
      end
    end

    return total
  end


  --| Quest_update_tvols |---  

  for _,C in ipairs(arena.conns) do
    C.src_tvol  = travel_volume(C.src,  { [C]=true })
    C.dest_tvol = travel_volume(C.dest, { [C]=true })
  end
end


function Quest_initial_path(arena)

  -- TODO: preference for paths that contain many junctions
  --       [might be more significant than travel volume]

  local function select_next_room(R, path)
    local best_C
    local best_tvol = -1

    for _,C in ipairs(R.conns) do
      if C.src == R and not C.lock then
        if best_tvol < C.dest_tvol then
          best_tvol = C.dest_tvol
          best_C = C
        end
      end
    end

    if not best_C then
      return nil
    end

    table.insert(path, best_C)

    return best_C.dest
  end


  --| Quest_initial_path |--

  Quest_update_tvols(arena)

  arena.path = {}

  local R = arena.start

  while R do
    arena.target = R
    R = select_next_room(R, arena.path)
  end

  gui.debugf("Arena %s  start: S(%d,%d)  target: S(%d,%d)\n",
             tostring(arena), arena.start.sx1, arena.start.sy1,
             arena.target.sx1, arena.target.sy1)
end


function Quest_num_puzzles(num_rooms)
  local PUZZLE_MINS = { less=18, normal=12, more=8, mixed=16 }
  local PUZZLE_MAXS = { less=10, normal= 8, more=4, mixed=6  }

  if not PUZZLE_MINS[OB_CONFIG.puzzles] then
    gui.printf("Puzzles disabled\n")
    return 0
  end

do return 8 end --!!!!!!!!

  local p_min = num_rooms / PUZZLE_MINS[OB_CONFIG.puzzles]
  local p_max = num_rooms / PUZZLE_MAXS[OB_CONFIG.puzzles]

  local result = int(0.25 + rand_range(p_min, p_max))

  gui.printf("Number of puzzles: %d  (%1.2f-%1.2f) rooms=%d\n", result, p_min, p_max, num_rooms)

  return result
end


function PREVIOUS_Quest_lock_up_arena(arena)


  local function collect_arena(A, R)
    table.insert(A.rooms, R)

    for _,C in ipairs(R.conns) do
      if C.src == R and C == LC then
        collect_arena(back_A, C.dest)
      elseif C.src == R and not C.lock then
        table.insert(A.conns, C)
        collect_arena(A, C.dest)
      end
    end
  end

  collect_arena(front_A, arena.start)

end


function Quest_lock_up_arena(arena)

  local function eval_lock(C)

    -- Factors to consider:
    --
    -- 1) primary factor is how well this connection breaks up the
    --    arena: a 50/50 split is the theoretical ideal, however we
    --    actually go for 66/33 split, because locked doors are
    --    better closer to the exit room than the start room
    --    [extra space near the start room can be used for weapons
    --    and other pickups].
    --
    -- 2) try to avoid Outside-->Outside connections, since we
    --    cannot use keyed doors in DOOM without creating a tall
    --    (ugly) door frame.  Worse is when there is a big height
    --    difference.
    --
    -- 3) preference for "ON" locks (less back-tracking)

    assert(C.src_tvol and C.dest_tvol)

    local cost = math.abs(C.src_tvol - C.dest_tvol * 2)

    if C.src.outdoor and C.dest.outdoor then
      cost = cost + 40
    end

    if not C.on_path then
      cost = cost + 30
    end

    return cost + gui.random() * 5
  end

  local function add_lock(list, C)
    if not table_contains(list, C) then
      C.on_path = table_contains(arena.path, C)
      C.lock_cost = eval_lock(C)
      table.insert(list, C)
    end
  end

  local function locks_for_room(R, list)
    if R.is_junction then
      for _,C in ipairs(R.conns) do
        if C.src == R and not C.lock then
          add_lock(list, C)
        end
      end
    end
  end

  local function dump_locks(list)
    for _,C in ipairs(list) do
      gui.debugf("Lock S(%d,%d) --> S(%d,%d) cost=%1.2f\n",
                 C.src.sx1,C.src.sy1, C.dest.sx1,C.dest.sy1, C.lock_cost)
    end
  end

  local function dump_arena(A, name)
    gui.debugf("%s ARENA  %s  %d+%d\n", name, tostring(A), #A.rooms, #A.conns)
    gui.debugf("{\n")

    gui.debugf("  start room  S(%d,%d)\n",  A.start.sx1, A.start.sy1)
    gui.debugf("  target room S(%d,%d)\n", A.target.sx1, A.target.sy1)
    gui.debugf("  target item: %s\n", A.target_item or "??????")

    gui.debugf("  PATH:\n")
    gui.debugf("  {\n")

    for _,C in ipairs(A.path) do
      gui.debugf("  conn  %s  (%d,%d) -> (%d,%d)\n",
                 tostring(C), C.src.sx1, C.src.sy1, C.dest.sx1, C.dest.sy1)
    end

    gui.debugf("  }\n")
    gui.debugf("}\n")
  end


  ---| Quest_lock_up_arena |---

  dump_arena(arena, "INPUT")

  -- choose connection which will get locked
  local poss_locks = {}

  locks_for_room(arena.start, poss_locks)

  for _,C in ipairs(arena.path) do
    locks_for_room(C.dest, poss_locks)
  end
 
  assert(#poss_locks > 0)
  dump_locks(poss_locks)

  local LC = table_sorted_first(poss_locks, function(X,Y) return X.lock_cost < Y.lock_cost end)
  assert(LC)

  gui.debugf("Lock conn has COST:%1.2f on_path:%s\n",
             LC.lock_cost, sel(LC.on_path, "YES", "NO"))


  local LOCK =
  {
    conn = LC,
    tag  = PLAN:alloc_tag(),

    -- TEMP CRUD
    key_item = 1 + #PLAN.all_locks,
  }

  LC.lock = LOCK

  table.insert(PLAN.all_locks, LOCK)


-- temp crud for debugging
local KS = LC.dest_S
if KS and LC.src_S then
local dir = delta_to_dir(LC.src_S.sx - KS.sx, LC.src_S.sy - KS.sy)
KS.borders[dir].kind = "lock_door"
KS.borders[dir].key_item = LOCK.key_item
end


  --- perform split ---

  gui.debugf("Splitting arena, old sizes: %d+%d", #arena.rooms, #arena.conns)

  local front_A =
  {
    rooms = {},
    conns = {},
    start = arena.start,
    target_item = LOCK.key_item,
  }

  local back_A =
  {
    rooms = {},
    conns = {},
    start = LOCK.conn.dest,
    target_item = arena.target_item,
  }


  local function collect_arena(A, R)
    table.insert(A.rooms, R)

    for _,C in ipairs(R.conns) do
      if C.src == R and not C.lock then
        table.insert(A.conns, C)
        collect_arena(A, C.dest)
      end
    end
  end

  collect_arena(front_A, front_A.start)
  collect_arena(back_A,  back_A.start)


  Quest_initial_path(back_A)

  if not LC.on_path then
    -- this is easy (front path stays the same)
    front_A.target = arena.target
    front_A.path   = arena.path
  else
    -- create second half of front path
    front_A.start = LOCK.conn.src
    Quest_initial_path(front_A)
    front_A.start = arena.start

    -- add first half of path (upto the locked connection)
    local hit_lock = false
    for index,C in ipairs(arena.path) do
      if C == LOCK.conn then
        hit_lock = true; break;
      end
      table.insert(front_A.path, index, C)
    end

    assert(hit_lock)
  end


  -- link in the newbies...
  table.insert(PLAN.all_arenas, front_A)
  table.insert(PLAN.all_arenas, back_A)

  -- remove the oldie....
  for index,A in ipairs(PLAN.all_arenas) do
    if arena == A then
      table.remove(PLAN.all_arenas, index)
      break;
    end
  end

  gui.debugf("Successful split, new sizes: %d+%d | %d+%d\n",
             #front_A.rooms, #front_A.conns,
              #back_A.rooms,  #back_A.conns)

  dump_arena(front_A, "FRONT")
  dump_arena( back_A, "BACK")
end


function Quest_add_lock()

  local function room_is_junction(R)
    local count = 0

    for _,C in ipairs(R.conns) do
      if C.src == R and not C.lock then
        count = count + 1
      end
    end

    return (count >= 2)
  end

  local function eval_arena(arena)
    -- count junctions along path
    local R = arena.start
    local junctions = sel(R.is_junction, 1, 0)

    for _,C in ipairs(arena.path) do
      if C.dest.is_junction then
        junctions = junctions + 1
      end
    end

    -- a lock is impossible without a junction
    if junctions == 0 then
      return -1
    end

    local score = junctions + gui.random()

    return score
  end


  --| Quest_add_lock |--

  for _,R in ipairs(PLAN.all_rooms) do
    R.is_junction = room_is_junction(R)
  end

  -- choose arena to add the locked door into
  for _,A in ipairs(PLAN.all_arenas) do
    A.split_score = eval_arena(A)
gui.debugf("Arena %s  split_score:%1.4f\n", tostring(A), A.split_score)
  end

  local arena = table_sorted_first(PLAN.all_arenas, function(X,Y) return X.split_score > Y.split_score end)

  if arena.split_score < 0 then
    gui.debugf("No more locks could be made!\n")
    return
  end

  Quest_update_tvols(arena)

  Quest_lock_up_arena(arena)
end


function PREVIOUS_Quest_solve_puzzles()

  local function mark_locked_paths(R)
    -- each free connection will be marked if it leads to a
    -- room which contains a locked door.

    local has_lock = false

    for _,C in ipairs(R) do
      if C.lock then
        table.insert(lock_list, C)
        C.leading = nil
        has_lock = true

      elseif C.src == R then
        if mark_locked_paths(C.dest, lock_list) then
          C.leading = true
          has_lock = true
        end
      end
    end -- for C

    return has_lock
  end

  local function collect_locks(R, path, lock_list)
    for index = 0,#path do
      if index > 0 then
        R = path[index].dest
      end

      for _,C in ipairs(R) do
        if C.lock then
          table.insert(lock_list, C)
        end
      end
    end
  end

  local function path_to_lock(R, LC)
    if R == LC.src then
      return {}
    end

    -- try each outward connection, one of them may succeed
    for _,C in ipairs(R.conns) do
      if C.src == R and not C.lock then
        local path = path_to_lock(C:neighbor(R), LC)
        if path then
          table.insert(path, 1, C)
          return path
        end
      end
    end

    return nil -- did not exist
  end

--[[  local function select_target(R)
    for loop = 1,50 do
      local poss_bras = {}

      for _,C in ipairs(R.conns) do
        if C.src == R and not C.lock then
          table.insert(poss_bras, C)
        end
      end

      if #poss_bras == 0 then
        break;
      end

      branch_C = table_sorted_first(poss_bras,
          function(A,B) return A.dest_tvol > B.dest_tvol end)
      assert(branch_C)

      -- move into chosen room
      table.insert(arena.path, branch_C)
      R = branch_C.dest
    end

    return R
  end --]]

  local function find_target(R, path)
    while true do
      local poss_bras = {}

      for _,C in ipairs(R.conns) do
        if C.src == R and not C.lock then
          table.insert(poss_bras, C)
        end
      end

      if #poss_bras == 0 then
        break;
      end

      branch_C = table_sorted_first(poss_bras,
          function(A,B)
            if A.leading ~= B.leading then
              return sel(A.leading,1,0) > sel(B.leading,1,0)
            end
            return A.dest_tvol > B.dest_tvol
          end)

      -- move into chosen room
      table.insert(path, branch_C)
      R = branch_C.dest
    end

    return R
  end


  --| Quest_solve_puzzle |--
 
  Quest_update_tvols(arena)

---  for _,A in ipairs(PLAN.all_arenas) do
---    A.path = {}
---    A.target = select_target(A.start)
---  end


  local arena = PLAN.all_arenas[1]
  assert(arena)

  local lock_list = {}


  while true do
    local has_lock = mark_locked_paths(arena.start, lock_list)

    arena.path = {}
    arena.target = find_target(arena.start, arena.path)

    if has_lock then
      collect_locks(arena.start, arena.path, lock_list)
    end

    -- no more locked doors anywhere?
    if #lock_list == 0 then
      arena.target.purpose = "EXIT"
      PLAN.exit_room = arena.target

-- TEMP CRUD
local ex = int((arena.target.sx1 + arena.target.sx2) / 2.0)
local ey = int((arena.target.sy1 + arena.target.sy2) / 2.0)
SEEDS[ex][ey][1].is_exit = true

      break;
    end

    local idx = rand_irange(1,#lock_list) --FIXME !!!!

    local LC = table.remove(lock_list, idx)

    


  end -- while true



  front_A.path = path_to_lock(arena.start, LC)
  assert(front_A.path)
    

  if arena.lock then
    local LOCK = arena.lock
assert(arena.path)

    arena.target = select_target(LOCK.conn.src)
  else
    -- the EXIT room
    arena.path = {}
    arena.target = select_target(arena.start)
  end 
end


function Quest_add_keys()

  for _,arena in ipairs(PLAN.all_arenas) do
    local R = arena.target
    assert(R)
    assert(arena.target_item)

    if arena.target_item == "EXIT" then
      PLAN.exit_room = R
      R.purpose = "EXIT"

-- TEMP CRUD
local ex = int((R.sx1 + R.sx2) / 2.0)
local ey = int((R.sy1 + R.sy2) / 2.0)
SEEDS[ex][ey][1].is_exit = true

    else
-- TEMP CRUD
R.key_item = arena.target_item
    end
  end
end


function Quest_assign()

  gui.printf("\n--==| Quest_assign |==--\n\n")

  -- need at least a START room and an EXIT room
  assert(#PLAN.all_rooms >= 2)

  -- count branches in each room
  for _,R in ipairs(PLAN.all_rooms) do
    R.sw, R.sh = box_size(R.sx1, R.sy1, R.sx2, R.sy2)
    R.svolume  = (R.sw+1) * (R.sh+1) / 2

    if R.kind ~= "scenic" then
      R.num_branch = #R.conns + #R.teleports
      if R.num_branch == 0 then
        error("Room exists with no connections!")
      end
gui.printf("Room (%d,%d) branches:%d\n", R.lx1,R.ly1, R.num_branch)
    end
  end

  PLAN.num_puzz = Quest_num_puzzles(#PLAN.all_rooms)


  local ARENA =
  {
    rooms = {},
    conns = copy_table(PLAN.all_conns),
    target_item = "EXIT",
  }

  for _,R in ipairs(PLAN.all_rooms) do
    if R.kind ~= "scenic" then
      table.insert(ARENA.rooms, R)
    end
  end


  PLAN.all_arenas = { ARENA }
  PLAN.all_locks  = {}

  Quest_decide_start_room(ARENA)

  PLAN.start_room = PLAN.all_arenas[1].start
  PLAN.start_room.purpose = "START"


  Quest_initial_path(ARENA)

  for i = 1,PLAN.num_puzz do
    Quest_add_lock()
  end

  for _,A in ipairs(PLAN.all_arenas) do
    for _,R in ipairs(A.rooms) do
      R.arena = A
    end
  end

  Quest_add_keys()


  -- TEMP CRUD FOR BUILDER....


  local START_R = PLAN.start_room
  assert(START_R)

  local sx = int((START_R.sx1 + START_R.sx2) / 2.0)
  local sy = int((START_R.sy1 + START_R.sy2) / 2.0)


  SEEDS[sx][sy][1].is_start = true

  gui.printf("Start seed @ (%d,%d)\n", sx, sy)


--[[ !!!!
  local EXIT_R = PLAN.exit_room
  assert(EXIT_R)
  assert(EXIT_R ~= START_R)

  local ex = int((EXIT_R.sx1 + EXIT_R.sx2) / 2.0)
  local ey = int((EXIT_R.sy1 + EXIT_R.sy2) / 2.0)


  SEEDS[ex][ey][1].is_exit = true

  gui.printf("Exit seed @ (%d,%d)\n", ex, ey)
--]]
end

