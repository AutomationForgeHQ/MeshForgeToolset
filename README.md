# MeshForge Toolset

Exposes [MeshForge](https://github.com/AutomationForgeHQ/MeshForge) to agents through the Unreal toolset registry.

**Version 0.2.1. Experimental.**

An adapter and nothing more. Every tool forwards to `UMeshForgeSubsystem`, which owns the pipeline;
deleting this plugin changes nothing about how MeshForge behaves.

## Why it is a separate plugin

`ToolsetRegistry` and `ModelContextProtocol` are Experimental engine plugins. Folding the adapter
into MeshForge would make MeshForge refuse to load in any project where they are switched off,
which breaks its "drops into any UE 5.8 project" goal.

It depends on `ToolsetRegistry`, not `ModelContextProtocol` — MCP is one transport reading that
registry, and coupling to it would be backwards.

## The tools

| | |
|---|---|
| `ListMeshProviders` | What is installed, what each can do, and whether it can generate right now. **Call this first** — providers differ in whether they accept text at all. |
| `ListMeshDefinitions` / `GetMeshDefinition` | Find definitions and read their state. |
| `CreateMeshDefinition` | Author one. Does not generate. |
| `SetMeshControl` / `SetMeshFinishSettings` | Change how it generates, and how it is finished. |
| `EstimateMeshGeneration` | What a batch would cost, before spending it. |
| `GenerateMeshes` | The call that spends. Returns when every definition has finished or failed. |
| `ImportMeshCandidate` | Import a different take than the one chosen automatically. |
| `RefinishMesh` | Re-apply collision, scale, pivot, Nanite and lightmap UVs. Free. |
| `ImportMeshFile` | Import a `.glb`/`.gltf` already on disk — a web tool, a scan, a colleague's file — finished the same way a generated mesh is. |
| `ListPipelines` | Every pipeline class a stage could be set to, with what it costs and what it can be told. Call before `SetPipeline`. |
| `SetPipeline` | Put a pipeline on one of a definition's stages, replacing whatever was there. Marks later stages stale without discarding what they produced. |
| `DrawConceptImage` | Draw a reference image through a definition's concept pipeline. The largest quality lever in the pipeline, and spends on a hosted pipeline. |
| `ListDefinitionImages` | Every picture a definition holds, and which of them its generator is shown. |
| `ChooseImages` | Choose the main image and up to three extra views the mesh generator is shown. |
| `RunPostProcessing` | Run a definition's post-processing chain (or one step of it), then re-import what comes out. The call that spends on a metered Post step. |
| `ListPostInputs` | List saved post outputs, including older takes predating per-step history, so another input can be selected. |
| `SetPostPipelines` | Set a definition's whole post-processing chain, in order, replacing whatever was there — order is the meaning. |
| `ListJobs` | What MeshForge is doing right now, and what it recently finished. |

Signatures are the schema: parameters and returns are `USTRUCT`s and enums rather than JSON
strings, so the registry publishes something an agent can fill in without guessing field names, and
the doc comments become the descriptions it reads.

Failures raise a script error rather than returning an ok/error envelope, and every message names
the tool to call next.

## Deliberately absent

- **No tool writes a credential.** An agent that can spend somebody's API budget should not also be
  able to store the key that pays for it. Entering a key is a human action, performed once.
- **No tool deletes a generated asset.** A mesh that took a minute of GPU time should be deleted by
  a person in the Content Browser.

Both are design decisions rather than omissions.

## The skill

`UMeshForgeSkill` carries what no signature can say: which provider can take a prompt, what a
generated mesh is like before it is finished, and which changes are free. Native skills are
discovered automatically — there is nothing to register.
