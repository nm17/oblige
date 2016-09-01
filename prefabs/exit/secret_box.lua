--
--  Secret closet for a secret exit
--

PREFABS.Exit_secret_box1 =
{
  file  = "exit/secret_box.wad"
  where = "seeds"

  kind  = "secret_exit"

  seed_w = 1
  seed_h = 1

  x_fit = "frame"
  y_fit = "top"

  prob  = 100

  thing_34 =
  {
    dead_player = 10
    gibbed_player = 10
    dead_shooter = 10
    dead_imp = 10
    dead_demon = 10
    dead_cacodemon = 10
  }
}
