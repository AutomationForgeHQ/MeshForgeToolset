#include "MeshForgeSkill.h"

UMeshForgeSkill::UMeshForgeSkill()
{
	Description = TEXT(
		"How to generate 3D props with MeshForge: which provider can take a prompt, what a generated "
		"mesh is like before it is finished, and which changes are free.");

	Instructions = TEXT(
		"Generated meshes are props. Ask for one object, not a scene - the model reconstructs "
		"everything it is shown, so a crate photographed on a table gives you a crate welded to a "
		"table.\n"
		"\n"
		"Start by listing the providers, because they differ in the one way that decides how you "
		"write a definition: some make a mesh from a text prompt, and some only from an image. Where "
		"a provider needs an image and you only have words, check whether it can draw its own - that "
		"is a separate capability, off unless somebody turned it on, and without it a prompt-only "
		"definition on an image-only provider simply cannot run. Say so rather than generating "
		"something else.\n"
		"\n"
		"Describe the object, its material and its condition, and keep the framing plain. Studio "
		"lighting, dramatic angles and interesting backgrounds all end up as geometry.\n"
		"\n"
		"Generating is the expensive step and on a hosted provider it is the one that bills, per "
		"take, whether or not the take is kept. Estimate first, and leave the number of variants at "
		"one unless the provider is local and therefore free. Everything after generation - "
		"collision, pivot, scale, Nanite, lightmap UVs - costs nothing to change and can be redone "
		"any number of times, so when a mesh is nearly right, re-finish it rather than regenerating "
		"it. Regenerating with the same seed and settings gives the same mesh; changing the seed is "
		"how you ask for a different one.\n"
		"\n"
		"What arrives is a surface pulled out of a voxel field. It is dense, it is normalised into a "
		"unit box with no notion of real size, and it is centred on nothing in particular. That is "
		"why every definition carries a scale and a pivot choice, and why they matter more than they "
		"look: a prop at the wrong scale is the single most common thing wrong with a generated "
		"asset, and no amount of regenerating fixes it.\n"
		"\n"
		"Read the warnings on an import rather than only its success. An import that produced no "
		"textures, or could not fit collision, reports success with a warning - it is a usable mesh "
		"and it is not what was asked for. Convex decomposition is the collision worth having for "
		"anything a character touches, and it is also the one that fails on meshes with holes or "
		"loose shells; a single convex hull is the fallback that always works.\n"
		"\n"
		"Generation produces static surfaces. The optional garment extension can create a separate "
		"skeletal wardrobe asset by transferring weights from an existing character body onto a wrapped "
		"static mesh; it does not rig arbitrary generated characters. Preserve the wrapped output. "
		"When iterating on skinning, run only that step and select an existing wrapped output. "
		"Earlier steps are never run implicitly, and older saved takes can be chosen explicitly.");
}
