--
-- Outdoor teleporter platform
--

PREFABS.Teleporter_sky_pad =
{
  file   = "teleporter/sky_pad.wad"
  map    = "MAP01"

  rank   = 2
  prob   = 50

  env    = "outdoor"
  open_to_sky = 1

  where  = "seeds"
  seed_w = 2
  seed_h = 2

  x_fit  = "frame"

  tag_1 = "?out_tag"
  tag_2 = "?in_tag"
}
