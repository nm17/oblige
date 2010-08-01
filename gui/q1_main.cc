//------------------------------------------------------------------------
//  LEVEL building - QUAKE 1 format
//------------------------------------------------------------------------
//
//  Oblige Level Maker
//
//  Copyright (C) 2006-2010 Andrew Apted
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "headers.h"
#include "hdr_fltk.h"
#include "hdr_lua.h"
#include "hdr_ui.h"

#include "lib_file.h"
#include "lib_util.h"
#include "lib_pak.h"
#include "main.h"

#include "q_common.h"
#include "q_light.h"

#include "csg_main.h"
#include "csg_local.h"
#include "csg_quake.h"

#include "ui_chooser.h"
#include "img_all.h"

#include "q1_main.h"
#include "q1_structs.h"


q1MapModel_c::q1MapModel_c() :
    x1(0), y1(0), z1(0),
    x2(0), y2(0), z2(0),
    x_face(), y_face(), z_face()
{
  for (int i = 0; i < 4; i++)
    nodes[i] = 0;
}

q1MapModel_c::~q1MapModel_c()
{ }

std::vector<q1MapModel_c *> q1_all_mapmodels;


static char *level_name;
static char *description;


void Q1_CreateEntities(void)
{
  qLump_c *lump = BSP_NewLump(LUMP_ENTITIES);

  /* add the worldspawn entity */

  lump->Printf("{\n");

  lump->KeyPair("_generator", "OBLIGE " OBLIGE_VERSION " (c) Andrew Apted");
  lump->KeyPair("_homepage", "http://oblige.sourceforge.net");

  if (description)
    lump->KeyPair("message", description);
  else
    lump->KeyPair("message", "Oblige Level");

  lump->KeyPair("worldtype", "0");
  lump->KeyPair("classname", "worldspawn");

  lump->Printf("}\n");

  // add everything else

  for (unsigned int j = 0; j < all_entities.size(); j++)
  {
    entity_info_c *E = all_entities[j];

    lump->Printf("{\n");

    // write entity properties
    csg_property_set_c::iterator PI;
    for (PI = E->props.begin(); PI != E->props.end(); PI++)
    {
      lump->KeyPair(PI->first.c_str(), "%s", PI->second.c_str());
    }

    if ((I_ROUND(E->x) | I_ROUND(E->y) | I_ROUND(E->z)) != 0)
      lump->KeyPair("origin", "%1.1f %1.1f %1.1f", E->x, E->y, E->z);

    lump->KeyPair("classname", E->name.c_str());

    lump->Printf("}\n");
  }

  // add a trailing nul
  u8_t zero = 0;

  lump->Append(&zero, 1);
}


//------------------------------------------------------------------------

static std::vector<std::string>   q1_miptexs;
static std::map<std::string, int> q1_miptex_map;

s32_t Q1_AddMipTex(const char *name);

static void ClearMipTex(void)
{
  q1_miptexs.clear();
  q1_miptex_map.clear();

  // built-in textures
  Q1_AddMipTex("error");   // #0
  Q1_AddMipTex("missing"); // #1
  Q1_AddMipTex("o_carve"); // #2
}

s32_t Q1_AddMipTex(const char *name)
{
  if (q1_miptex_map.find(name) != q1_miptex_map.end())
  {
    return q1_miptex_map[name];
  }

  int index = (int)q1_miptexs.size();

  q1_miptexs.push_back(name);
  q1_miptex_map[name] = index;

  return index;
}

static void CreateDummyMip(qLump_c *lump, const char *name, int pix1, int pix2)
{
  SYS_ASSERT(strlen(name) < 16);

  miptex_t mm_tex;

  strcpy(mm_tex.name, name);

  int size = 64;

  mm_tex.width  = LE_U32(size);
  mm_tex.height = LE_U32(size);

  int offset = sizeof(mm_tex);

  for (int i = 0; i < MIP_LEVELS; i++)
  {
    mm_tex.offsets[i] = LE_U32(offset);

    offset += (size * size);
    size /= 2;
  }

  lump->Append(&mm_tex, sizeof(mm_tex));


  u8_t pixels[2] = { pix1, pix2 };

  size = 64;

  for (int i = 0; i < MIP_LEVELS; i++)
  {
    for (int y = 0; y < size; y++)
    for (int x = 0; x < size; x++)
    {
      lump->Append(pixels + (((x^y) & (size/4)) ? 1 : 0), 1);
    }

    size /= 2;
  }
}

static void CreateLogoMip(qLump_c *lump, const char *name, const byte *data)
{
  SYS_ASSERT(strlen(name) < 16);

  miptex_t mm_tex;

  strcpy(mm_tex.name, name);

  int size = 64;

  mm_tex.width  = LE_U32(size);
  mm_tex.height = LE_U32(size);

  int offset = sizeof(mm_tex);

  for (int i = 0; i < MIP_LEVELS; i++)
  {
    mm_tex.offsets[i] = LE_U32(offset);

    offset += (size * size);
    size /= 2;
  }

  lump->Append(&mm_tex, sizeof(mm_tex));


  size = 64;
  int scale = 1;

  static byte colormap[8] =
  {
    // 0, 16, 97, 101, 105, 109, 243, 243
    16, 97, 103, 109, 243, 243, 243, 243
  };

  for (int i = 0; i < MIP_LEVELS; i++)
  {
    for (int y = 0; y < size; y++)
    for (int x = 0; x < size; x++)
    {
      byte pixel = colormap[data[(63-y*scale)*64 + x*scale] >> 5];

      lump->Append(&pixel, 1);
    }

    size  /= 2;
    scale *= 2;
  }
}

static void TransferOneMipTex(qLump_c *lump, unsigned int m, const char *name)
{
  if (strcmp(name, "error") == 0)
  {
    CreateDummyMip(lump, name, 210, 231);
    return;
  }
  if (strcmp(name, "missing") == 0)
  {
    CreateDummyMip(lump, name, 4, 12);
    return;
  }
  if (strcmp(name, "o_carve") == 0)  // TEMP STUFF !!!!
  {
    CreateLogoMip(lump, name, logo_RELIEF.data);
    return;
  }

  int entry = WAD2_FindEntry(name);

  if (entry >= 0)
  {
    int pos    = 0;
    int length = WAD2_EntryLen(entry);

    byte buffer[1024];

    while (length > 0)
    {
      int actual = MIN(1024, length);

      if (! WAD2_ReadData(entry, pos, actual, buffer))
        Main_FatalError("Error reading texture data in wad!");

      lump->Append(buffer, actual);

      pos    += actual;
      length -= actual;
    }

    // all good
    return;
  }

  // not found!
  LogPrintf("WARNING: texture '%s' not found in texture wad!\n", name);

  CreateDummyMip(lump, name, 4, 12);
}

static void Q1_CreateMipTex(void)
{
  qLump_c *lump = BSP_NewLump(LUMP_TEXTURES);

  if (! WAD2_OpenRead("data/quake_tex.wd2"))
  {
    // FIXME: specified by a Lua function
    //        (do a check there, point user to website if not present)
    Main_FatalError("No such file: data/quake_tex.wd2");
    return; /* NOT REACHED */
  }

  u32_t num_miptex = q1_miptexs.size();
  u32_t dir_size = 4 * num_miptex + 4;
  SYS_ASSERT(num_miptex > 0);

  u32_t *offsets = new u32_t[num_miptex];

  for (unsigned int m = 0; m < q1_miptexs.size(); m++)
  {
    offsets[m] = dir_size + (u32_t)lump->GetSize();

    TransferOneMipTex(lump, m, q1_miptexs[m].c_str());
  }

  WAD2_CloseRead();

  // create miptex directory
  // FIXME: endianness
  lump->Prepend(offsets, 4 * num_miptex);
  lump->Prepend(&num_miptex, 4);

  delete[] offsets;
}

#if 0  /* TEMP DUMMY STUFF */
static void DummyMipTex(void)
{
  // 0 = "error"
  // 1 = "gray"

  qLump_c *lump = BSP_NewLump(LUMP_TEXTURES);


  dmiptexlump_t mm_dir;

  mm_dir.num_miptex = LE_S32(2);

  mm_dir.data_ofs[0] = LE_S32(sizeof(mm_dir));
  mm_dir.data_ofs[1] = LE_S32(sizeof(mm_dir) + sizeof(miptex_t) + 85*4);

  lump->Append(&mm_dir, sizeof(mm_dir));


  for (int mt = 0; mt < 2; mt++)
  {
    miptex_t mm_tex;

    strcpy(mm_tex.name, (mt == 0) ? "error" : "gray");

    int size = 16;

    mm_tex.width  = LE_U32(size);
    mm_tex.height = LE_U32(size);

    int offset = sizeof(mm_tex);

    for (int i = 0; i < MIP_LEVELS; i++)
    {
      mm_tex.offsets[i] = LE_U32(offset);

      offset += (u32_t)(size * size);

      size = size / 2;
    }

    lump->Append(&mm_tex, sizeof(mm_tex));


    u8_t pixels[2];

    pixels[0] = (mt == 0) ? 210 : 4;
    pixels[1] = (mt == 0) ? 231 : 12;

    size = 16;

    for (int i = 0; i < MIP_LEVELS; i++)
    {
      for (int y = 0; y < size; y++)
      for (int x = 0; x < size; x++)
      {
        lump->Append(pixels + (((x^y) & 2)/2), 1);
      }

      size = size / 2;
    }
  }
}
#endif

//------------------------------------------------------------------------

static std::vector<texinfo_t> q1_texinfos;

#define NUM_TEXINFO_HASH  32
static std::vector<u16_t> * texinfo_hashtab[NUM_TEXINFO_HASH];


static void ClearTexInfo(void)
{
  q1_texinfos.clear();

  for (int h = 0; h < NUM_TEXINFO_HASH; h++)
  {
    delete texinfo_hashtab[h];
    texinfo_hashtab[h] = NULL;
  }
}

static bool MatchTexInfo(const texinfo_t *A, const texinfo_t *B)
{
  if (A->miptex != B->miptex)
    return false;

  if (A->flags != B->flags)
    return false;

  for (int k = 0; k < 4; k++)
  {
    if (fabs(A->s[k] - B->s[k]) > 0.01)
      return false;

    if (fabs(A->t[k] - B->t[k]) > 0.01)
      return false;
  }

  return true; // yay!
}

u16_t Q1_AddTexInfo(const char *texture, int flags, double *s4, double *t4)
{
  // create texinfo structure
  texinfo_t tin;

  for (int k = 0; k < 4; k++)
  {
    tin.s[k] = s4[k];
    tin.t[k] = t4[k];
  }

  tin.miptex = Q1_AddMipTex(texture);
  tin.flags  = flags;


  // find an existing texinfo.
  // For speed we use a hash-table.
  int hash = (int)tin.miptex % NUM_TEXINFO_HASH;

  SYS_ASSERT(hash >= 0);

  if (! texinfo_hashtab[hash])
    texinfo_hashtab[hash] = new std::vector<u16_t>;

  std::vector<u16_t> *hashtab = texinfo_hashtab[hash];

  for (unsigned int i = 0; i < hashtab->size(); i++)
  {
    u16_t tin_idx = (*hashtab)[i];

    SYS_ASSERT(tin_idx < q1_texinfos.size());

    if (MatchTexInfo(&tin, &q1_texinfos[tin_idx]))
      return tin_idx;  // found it
  }


  // not found, so add new one
  u16_t tin_idx = q1_texinfos.size();

  if (tin_idx >= MAX_MAP_TEXINFO)
    Main_FatalError("Quake1 build failure: exceeded limit of %d TEXINFOS\n",
                    MAX_MAP_TEXINFO);

  q1_texinfos.push_back(tin);

  hashtab->push_back(tin_idx);

  return tin_idx;
}

static void Q1_CreateTexInfo(void)
{
  qLump_c *lump = BSP_NewLump(LUMP_TEXINFO);

  // FIXME: write separately, fix endianness as we go
 
  lump->Append(&q1_texinfos[0], q1_texinfos.size() * sizeof(texinfo_t));
}


#if 0  /* TEMP DUMMY STUFF */
static void DummyTexInfo(void)
{
  // 0 = "error" on PLANE_X / PLANE_ANYX
  // 1 = "error" on PLANE_Y / PLANE_ANYY
  // 2 = "error" on PLANE_Z / PLANE_ANYZ
  //
  // 3 = "gray"  on PLANE_X / PLANE_ANYX
  // 4 = "gray"  on PLANE_Y / PLANE_ANYY
  // 5 = "gray"  on PLANE_Z / PLANE_ANYZ

  qLump_c *lump = BSP_NewLump(LUMP_TEXINFO);

  float scale = 8.0;

  for (int T = 0; T < 6; T++)
  {
    int P = T % 3;

    texinfo_t tex;

    tex.s[0] = (P == PLANE_X) ? 0 : 1;
    tex.s[1] = (P == PLANE_X) ? 1 : 0;
    tex.s[2] = 0;
    tex.s[3] = 0;

    tex.t[0] = 0;
    tex.t[1] = (P == PLANE_Z) ? 1 : 0;
    tex.t[2] = (P == PLANE_Z) ? 0 : 1;
    tex.t[3] = 0;

    for (int k = 0; k < 3; k++)
    {
      tex.s[k] /= scale;
      tex.t[k] /= scale;

      // FIXME: endianness swap!
    }

    int flags = 0;

    tex.miptex = LE_S32(T / 3);
    tex.flags  = LE_S32(flags);

    lump->Append(&tex, sizeof(tex));
  }
}
#endif


//------------------------------------------------------------------------

static void Q1_WriteEdge(quake_vertex_c *A, quake_vertex_c *B)
{
  u16_t v1 = BSP_AddVertex(A->x, A->y, A->z);
  u16_t v2 = BSP_AddVertex(B->x, B->y, B->z);

  if (v1 == v2)
  {
    Main_FatalError("INTERNAL ERROR: Q1 WriteEdge is zero length!\n");
  }

  s32_t index = BSP_AddEdge(v1, v2);

  // fix endianness
  index = LE_S32(index);

  q1_surf_edges->Append(&index, sizeof(index));
  q1_total_surf_edges += 1;
}


static void Q1_WriteFace(quake_face_c *face)
{
  face->index = q1_total_faces;

  q1_total_faces += 1;


  dface_t raw_face;

  memset(&raw_face, 0, sizeof(raw_face));


  bool flipped;

  raw_face.planenum = BSP_AddPlane(&face->plane, &flipped);

  raw_face.side = flipped ? 1 : 0;


  unsigned int total_v = face->verts.size();

  raw_face.firstedge = q1_total_surf_edges;
  raw_face.numedges  = total_v;

  for (unsigned int i = 0 ; i < total_v ; i++)
  {
    Q1_WriteEdge(face->verts[i], face->verts[(i+1) % total_v]);
  }


  // FIXME !!!! texinfo


  raw_face.lightofs = 0 + (rand() & 16383); //!!!!!! TEST CRUD

  if (face->lmap)
    raw_face.lightofs = face->lmap->CalcOffset();


  // fix endianness
  raw_face.planenum  = LE_S16(raw_face.planenum);
  raw_face.side      = LE_S16(raw_face.side);
  raw_face.firstedge = LE_S32(raw_face.firstedge);
  raw_face.numedges  = LE_S16(raw_face.numedges);
  raw_face.texinfo   = LE_S16(raw_face.texinfo);
  raw_face.lightofs  = LE_S32(raw_face.lightofs);


  q1_faces->Append(&raw_face, sizeof(raw_face));
}


static void Q1_WriteMarkSurf(quake_face_c *face)
{
  SYS_ASSERT(face->index >= 0);

  // fix endianness
  u16_t index = LE_U16(face->index);

  q1_mark_surfs->Append(&index, sizeof(index));
  q1_total_mark_surfs += 1;
}


static void Q1_WriteLeaf(quake_leaf_c *leaf)
{
  if (leaf == qk_solid_leaf)
    return;


  dleaf_t raw_leaf;

  memset(&raw_leaf, 0, sizeof(raw_leaf));


  raw_leaf.contents = leaf->contents;
  raw_leaf.visofs   = -1;  // no visibility info


  // create the 'mark surfs'
  raw_leaf.first_marksurf = q1_total_mark_surfs;
  raw_leaf.num_marksurf   = 0;

  for (unsigned int i = 0 ; i < leaf->faces.size() ; i++)
  {
    Q1_WriteMarkSurf(leaf->faces[i]);

    raw_leaf.num_marksurf += 1;
  }

#if 0  ///?????
  {
    rFace_c *F = leaf->faces[i];

    // should have been in a node already
    if (F->index < 0)
      LogPrintf("WARNING: face found in leaf but not in node\n");
    else
      Q1_AddSurf(F->index, &raw_leaf);
  }
#endif


  for (int b = 0 ; b < 3 ; b++)
  {
    raw_leaf.mins[b] = I_ROUND(leaf->mins[b]) - 4;
    raw_leaf.maxs[b] = I_ROUND(leaf->maxs[b]) + 4;

    // fix endianness
    raw_leaf.mins[b] = LE_S16(raw_leaf.mins[b]);
    raw_leaf.maxs[b] = LE_S16(raw_leaf.maxs[b]);
  }


  // fix endianness of everything else
  raw_leaf.contents = LE_S32(raw_leaf.contents);
  raw_leaf.visofs   = LE_S32(raw_leaf.visofs);

  raw_leaf.first_marksurf = LE_U16(raw_leaf.first_marksurf);
  raw_leaf.num_marksurf   = LE_U16(raw_leaf.num_marksurf);


  q1_leafs->Append(&raw_leaf, sizeof(raw_leaf));
}


static void Q1_WriteNode(quake_node_c *node)
{
  dnode_t raw_node;

  bool flipped;

  raw_node.planenum = BSP_AddPlane(&node->plane, &flipped);

  
  if (node->front_N)
    raw_node.children[0] = (u16_t) node->front_N->index;
  else
    raw_node.children[0] = (u16_t) node->front_L->index;

  if (node->back_N)
    raw_node.children[1] = (u16_t) node->back_N->index;
  else
    raw_node.children[1] = (u16_t) node->back_L->index;

  if (flipped)
  {
    std::swap(raw_node.children[0], raw_node.children[1]);
  }


  if (node->faces.size() > 0)
  {
    raw_node.firstface = q1_total_faces;
    raw_node.numfaces  = node->faces.size();

    for (unsigned int k = 0 ; k < node->faces.size() ; k++)
    {
      Q1_WriteFace(node->faces[k], node);
    }
  }


  for (int b = 0 ; b < 3 ; b++)
  {
    raw_node.mins[b] = I_ROUND(node->mins[b]) - 32;
    raw_node.maxs[b] = I_ROUND(node->maxs[b]) + 32;

    // fix endianness
    raw_node.mins[b] = LE_S16(raw_node.mins[b]);
    raw_node.maxs[b] = LE_S16(raw_node.maxs[b]);
  }


  // fix endianness of everything else
  raw_node.planenum    = LE_S32(raw_node.planenum);
  raw_node.children[0] = LE_S16(raw_node.children[0]);
  raw_node.children[1] = LE_S16(raw_node.children[1]);
  raw_node.firstface   = LE_U16(raw_node.firstface);
  raw_node.numfaces    = LE_U16(raw_node.numfaces);


  q1_nodes->Append(&raw_node, sizeof(raw_node));


  // recurse now, AFTER adding the current node

  if (node->front_N)
    Q1_WriteNode(node->front_N);
  else
    Q1_WriteLeaf(node->front_L);

  if (node->back_N)
    Q1_WriteNode(node->back_N);
  else
    Q1_WriteLeaf(node->back_L);
}


static void Q1_WriteBSP()
{
  Q1_WriteNode(qk_bsp_root);  
}


//------------------------------------------------------------------------

class quake1_game_interface_c : public game_interface_c
{
private:
  const char *filename;

public:
  quake1_game_interface_c() : filename(NULL)
  { }

  ~quake1_game_interface_c()
  { }

  bool Start();
  bool Finish(bool build_ok);

  void BeginLevel();
  void EndLevel();
  void Property(const char *key, const char *value);
};


bool quake1_game_interface_c::Start()
{
  filename = Select_Output_File("pak");

  if (! filename)
  {
    Main_ProgStatus("Cancelled");
    return false;
  }

  if (create_backups)
    Main_BackupFile(filename, "old");

  if (! PAK_OpenWrite(filename))
  {
    Main_ProgStatus("Error (create file)");
    return false;
  }

  BSP_AddInfoFile();

  if (main_win)
    main_win->build_box->Prog_Init(0, "CSG,BSP,Hull 1,Hull 2" /*Light,Vis*/);

  return true;
}


bool quake1_game_interface_c::Finish(bool build_ok)
{
  PAK_CloseWrite();

  // remove the file if an error occurred
  if (! build_ok)
    FileDelete(filename);

  return build_ok;
}


void quake1_game_interface_c::BeginLevel()
{
  level_name  = NULL;
  description = NULL;
}


void quake1_game_interface_c::Property(const char *key, const char *value)
{
  if (StringCaseCmp(key, "level_name") == 0)
  {
    level_name = StringDup(value);
  }
  else if (StringCaseCmp(key, "description") == 0)
  {
    description = StringDup(value);
  }
  else
  {
    LogPrintf("WARNING: QUAKE1: unknown level prop: %s=%s\n", key, value);
  }
}


void quake1_game_interface_c::EndLevel()
{
  if (! level_name)
    Main_FatalError("Script problem: did not set level name!\n");

  if (strlen(level_name) >= 32)
    Main_FatalError("Script problem: level name too long: %s\n", level_name);

  char entry_in_pak[64];
  sprintf(entry_in_pak, "maps/%s.bsp", level_name);

  BSP_OpenLevel(entry_in_pak, 1);

  ClearMipTex();
  ClearTexInfo();

  BSP_PreparePlanes  (LUMP_PLANES,   MAX_MAP_PLANES);
  BSP_PrepareVertices(LUMP_VERTEXES, MAX_MAP_VERTS);
  BSP_PrepareEdges   (LUMP_EDGES,    MAX_MAP_EDGES);

  BSP_InitLightmaps();

  CSG_QUAKE_Build();

  Q1_CreateMipTex();
  Q1_CreateTexInfo();
  Q1_CreateEntities();

  BSP_WritePlanes();
  BSP_WriteVertices();
  BSP_WriteEdges();

  BSP_BuildLightmap(LUMP_LIGHTING, MAX_MAP_LIGHTING, false);

  BSP_CloseLevel();

  // FREE STUFF !!!!

  StringFree(level_name);

  if (description)
    StringFree(description);

  BSP_FreeLightmaps();
}


game_interface_c * Quake1_GameObject(void)
{
  return new quake1_game_interface_c();
}

//--- editor settings ---
// vi:ts=2:sw=2:expandtab
