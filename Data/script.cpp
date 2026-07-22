using Microsoft.Xna.Framework;
using Microsoft.Xna.Framework.Graphics;
using SVSAP;
using StardewModdingAPI;
using StardewModdingAPI.Events;
using StardewValley;
using StardewValley.GameData.BigCraftables;
using StardewValley.GameData.FloorsAndPaths;
using StardewValley.GameData.Objects;

namespace SVSAP.Content;

internal sealed class ContentInjector
{
    private const string FreeRecipeIngredientList = "(O)388 0";
    private const string ObjectSpriteAsset = "Mods/" + ModItemCatalog.UniqueId + "/Items";
    private const string BigCraftableSpriteAsset = "Mods/" + ModItemCatalog.UniqueId + "/BigCraftables";
    private const string ObjectSpriteResource = "SVSAP.Assets.Items.png";
    private const string BigCraftableSpriteResource = "SVSAP.Assets.BigCraftables.png";

    private readonly Func<ModConfig> getConfig;
    private readonly IMonitor monitor;

    public ContentInjector(Func<ModConfig> getConfig, IMonitor monitor)
    {
        this.getConfig = getConfig;
        this.monitor = monitor;
    }

    public void OnAssetRequested(object? sender, AssetRequestedEventArgs e)
    {
        if (e.NameWithoutLocale.IsEquivalentTo(ObjectSpriteAsset))
        {
            e.LoadFrom(() => LoadEmbeddedTexture(ObjectSpriteResource), AssetLoadPriority.Exclusive);
            return;
        }

        if (e.NameWithoutLocale.IsEquivalentTo(BigCraftableSpriteAsset))
        {
            e.LoadFrom(() => LoadEmbeddedTexture(BigCraftableSpriteResource), AssetLoadPriority.Exclusive);
            return;
        }

        if (e.NameWithoutLocale.IsEquivalentTo("Data/Objects"))
        {
            e.Edit(asset =>
            {
                var data = asset.AsDictionary<string, ObjectData>().Data;

                var spriteIndex = 0;
                foreach (var item in ModItemCatalog.ObjectItems)
                {
                    var localKey = ModItemCatalog.GetLocalKey(item.Id);
                    data[item.Id] = new ObjectData
                    {
                        Name = item.Name,
                        DisplayName = ModText.Get("items." + localKey + ".name", item.DisplayName),
                        Description = ModText.Get("items." + localKey + ".description", item.Description),
                        Type = "Basic",
                        Category = item.Category,
                        Price = item.Price,
                        Texture = ObjectSpriteAsset,
                        SpriteIndex = spriteIndex++,
                        Edibility = -300,
                        ContextTags = item.ContextTags.ToList()
                    };
                }
            });

            return;
        }

        if (e.NameWithoutLocale.IsEquivalentTo("Data/BigCraftables"))
        {
            e.Edit(asset =>
            {
                var data = asset.AsDictionary<string, BigCraftableData>().Data;

                var spriteIndex = 0;
                foreach (var item in ModItemCatalog.BigCraftables)
                {
                    var localKey = ModItemCatalog.GetLocalKey(item.Id);
                    data[item.Id] = new BigCraftableData
                    {
                        Name = item.Name,
                        DisplayName = ModText.Get("machines." + localKey + ".name", item.DisplayName),
                        Description = ModText.Get("machines." + localKey + ".description", item.Description),
                        Price = item.Price,
                        Fragility = 0,
                        CanBePlacedIndoors = true,
                        CanBePlacedOutdoors = true,
                        Texture = BigCraftableSpriteAsset,
                        SpriteIndex = spriteIndex++,
                        ContextTags = new List<string> { "color_gray" }
                    };
                }
            });

            return;
        }

        if (e.NameWithoutLocale.IsEquivalentTo("Data/CraftingRecipes"))
        {
            e.Edit(asset =>
            {
                var data = asset.AsDictionary<string, string>().Data;
                foreach (var pair in this.GetCraftingRecipes())
                    data[pair.Key] = pair.Value;
            });

            return;
        }

        if (e.NameWithoutLocale.IsEquivalentTo("Data/FloorsAndPaths"))
        {
            e.Edit(asset =>
            {
                var data = asset.AsDictionary<string, FloorPathData>().Data;
                data[ModItemCatalog.NetworkCable] = new FloorPathData
                {
                    Id = ModItemCatalog.NetworkCable,
                    ItemId = ModItemCatalog.NetworkCable,
                    Texture = "Maps/springobjects",
                    Corner = new Point(2 * 16, 14 * 16),
                    PlacementSound = "axchop",
                    RemovalSound = "axchop",
                    RemovalDebrisType = 4,
                    FootstepSound = "stoneStep",
                    ConnectType = FloorPathConnectType.Path,
                    ShadowType = FloorPathShadowType.None,
                    FarmSpeedBuff = -1
                };
            });
        }
    }

    private IEnumerable<KeyValuePair<string, string>> GetCraftingRecipes()
    {
        var recipeCostMode = this.getConfig().GetRecipeCostMode();
        foreach (var pair in ModItemCatalog.CraftingRecipes)
        {
            var raw = recipeCostMode switch
            {
                RecipeCostModes.Debug => MakeRecipeFree(pair.Value),
                RecipeCostModes.Casual => ReduceIngredientCosts(pair.Value),
                _ => pair.Value
            };

            yield return new KeyValuePair<string, string>(
                pair.Key,
                recipeCostMode == RecipeCostModes.Debug ? raw : ApplyMiningRequirement(pair.Key, raw));
        }
    }

    private static string MakeRecipeFree(string raw)
    {
        var parts = raw.Split('/');
        if (parts.Length == 0)
            return raw;

        parts[0] = FreeRecipeIngredientList;
        return string.Join("/", parts);
    }

    private static string ReduceIngredientCosts(string raw)
    {
        var parts = raw.Split('/');
        if (parts.Length == 0)
            return raw;

        var tokens = parts[0].Split(' ', StringSplitOptions.RemoveEmptyEntries);
        if (tokens.Length < 2)
            return raw;

        var reduced = new List<string>();
        for (var i = 0; i + 1 < tokens.Length; i += 2)
        {
            reduced.Add(tokens[i]);
            if (int.TryParse(tokens[i + 1], out var count) && count > 1)
                reduced.Add(Math.Max(1, (count + 1) / 2).ToString());
            else
                reduced.Add(tokens[i + 1]);
        }

        if (tokens.Length % 2 != 0)
            reduced.Add(tokens[^1]);

        parts[0] = string.Join(" ", reduced);
        return string.Join("/", parts);
    }

    private static string ApplyMiningRequirement(string recipeName, string raw)
    {
        var requiredLevel = ModItemCatalog.GetRequiredMiningLevel(recipeName);
        if (requiredLevel <= 0)
            return raw;

        var parts = raw.Split('/');
        if (parts.Length < 5)
            return raw;

        parts[4] = $"Mining {requiredLevel}";
        return string.Join("/", parts);
    }

    private static Texture2D LoadEmbeddedTexture(string resourceName)
    {
        var assembly = typeof(ContentInjector).Assembly;
        using var stream = assembly.GetManifestResourceStream(resourceName)
            ?? throw new InvalidOperationException($"Missing embedded Stardew Valley Storage and Automation Project texture resource: {resourceName}");
        return Texture2D.FromStream(Game1.graphics.GraphicsDevice, stream);
    }
}
