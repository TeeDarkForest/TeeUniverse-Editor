/*
 * Copyright (C) 2016 necropotame (necropotame@gmail.com)
 * 
 * This file is part of TeeUniverse.
 * 
 * TeeUniverse is free software: you can redistribute it and/or  modify
 * it under the terms of the GNU Affero General Public License, version 3,
 * as published by the Free Software Foundation.
 *
 * TeeUniverse is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with TeeUniverse.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "autolayer.h"

#include <random>

void ApplyTilingMaterials(CAssetsManager* pAssetsManager, CAssetPath LayerPath, int Token)
{
	CAsset_MapLayerTiles* pLayer = pAssetsManager->GetAsset_Hard<CAsset_MapLayerTiles>(LayerPath);
	if(!pLayer)
		return;
	
	pAssetsManager->SaveAssetInHistory(LayerPath, Token);
	
	array2d<CAsset_MapLayerTiles::CTile>& Tiles = pLayer->GetTileArray();
	
	//Step 1: copy zone layer in the brush
	const CAsset_MapZoneTiles* pSource = pAssetsManager->GetAsset<CAsset_MapZoneTiles>(pLayer->GetSourcePath());
	if(pSource)
	{
		Tiles.resize(pSource->GetTileWidth(), pSource->GetTileHeight());
		pLayer->SetPositionX(0);
		pLayer->SetPositionY(0);
		
		const array2d<CAsset_MapZoneTiles::CTile>& SrcTiles = pSource->GetTileArray();
		
		if(pLayer->GetStylePath().GetType() == CAsset_TilingMaterial::TypeId)
		{
			const CAsset_TilingMaterial* pMaterial = pAssetsManager->GetAsset<CAsset_TilingMaterial>(pLayer->GetStylePath());
			if(pMaterial)
			{
				const std::vector<CAsset_TilingMaterial::CZoneConverter>& ZoneConverter = pMaterial->GetZoneConverterArray();
				
				for(int j=0; j<Tiles.get_height(); j++)
				{
					for(int i=0; i<Tiles.get_width(); i++)
					{
						int IndexFound = -1;
						
						for(unsigned int z=0; z<ZoneConverter.size(); z++)
						{
							if(ZoneConverter[z].GetZoneTypePath() != pSource->GetZoneTypePath() || ZoneConverter[z].GetOldIndex() != SrcTiles.get_clamp(i, j).GetIndex())
								continue;
							
							IndexFound = ZoneConverter[z].GetNewIndex();
						}
						
						if(IndexFound)
							Tiles.get_clamp(i, j).SetBrush(IndexFound);
						else
							Tiles.get_clamp(i, j).SetBrush(0);
					}
				}
			}
		}
	}
	
	//Step 2: Create tiles from brush
	ApplyTilingMaterials_Tiles(pAssetsManager, Tiles, pLayer->GetStylePath(), 0);
}

void ApplyTilingMaterials_Tiles(CAssetsManager* pAssetsManager, array2d<CAsset_MapLayerTiles::CTile>& Tiles, CAssetPath StylePath, int Seed)
{	
	//Step 2: Create tiles from brush
	if(StylePath.GetType() == CAsset_TilingMaterial::TypeId)
	{
		const CAsset_TilingMaterial* pMaterial = pAssetsManager->GetAsset<CAsset_TilingMaterial>(StylePath);
		if(pMaterial)
		{
			std::mt19937 RandomEngine(Seed);
			std::uniform_real_distribution<float> Distribution(0.0f, 1.0f);
			
			for(int j=0; j<Tiles.get_height(); j++)
			{
				for(int i=0; i<Tiles.get_width(); i++)
				{
					RandomEngine.seed(Seed + j*65536 + i);
					
					int Index = 0;
					int Flags = 0x0;
				
					//Apply rules
					CAsset_TilingMaterial::CIteratorRule RuleIter;
					for(RuleIter = pMaterial->ReverseBeginRule(); RuleIter != pMaterial->ReverseEndRule(); ++RuleIter)
					{
						bool IsValidRule = true;
						const std::vector<CAsset_TilingMaterial::CRule::CCondition>& Conditions = pMaterial->GetRuleConditionArray(*RuleIter);
						for(unsigned int c=0; c<Conditions.size(); c++)
						{
							if(Conditions[c].GetType() == CAsset_TilingMaterial::CONDITIONTYPE_INDEX)
							{
								if(Tiles.get_clamp(i + Conditions[c].GetRelPosX(), j + Conditions[c].GetRelPosY()).GetBrush() != Conditions[c].GetValue())
								{
									IsValidRule = false;
									break;
								}
							}
							else if(Conditions[c].GetType() == CAsset_TilingMaterial::CONDITIONTYPE_NOTINDEX)
							{
								if(Tiles.get_clamp(i + Conditions[c].GetRelPosX(), j + Conditions[c].GetRelPosY()).GetBrush() == Conditions[c].GetValue())
								{
									IsValidRule = false;
									break;
								}
							}
							else if(Conditions[c].GetType() == CAsset_TilingMaterial::CONDITIONTYPE_LABEL)
							{
								bool LabelIndexFound = false;
								CSubPath LabelPath = CAsset_TilingMaterial::SubPath_Label(Conditions[c].GetValue());
								if(pMaterial->IsValidLabel(LabelPath))
								{
									const std::vector<uint8>& Indices = pMaterial->GetLabelIndexArray(LabelPath);
									for(unsigned int li=0; li<Indices.size(); li++)
									{
										if(Indices[li] == Tiles.get_clamp(i + Conditions[c].GetRelPosX(), j + Conditions[c].GetRelPosY()).GetBrush())
										{
											LabelIndexFound = true;
											break;
										}
									}
								}
									
								if(!LabelIndexFound)
								{
									IsValidRule = false;
									break;
								}
							}
							else if(Conditions[c].GetType() == CAsset_TilingMaterial::CONDITIONTYPE_NOTLABEL)
							{
								bool LabelIndexFound = false;
								CSubPath LabelPath = CAsset_TilingMaterial::SubPath_Label(Conditions[c].GetValue());
								if(pMaterial->IsValidLabel(LabelPath))
								{
									const std::vector<uint8>& Indices = pMaterial->GetLabelIndexArray(LabelPath);
									for(unsigned int li=0; li<Indices.size(); li++)
									{
										if(Indices[li] == Tiles.get_clamp(i + Conditions[c].GetRelPosX(), j + Conditions[c].GetRelPosY()).GetBrush())
										{
											LabelIndexFound = true;
											break;
										}
									}
								}
									
								if(LabelIndexFound)
								{
									IsValidRule = false;
									break;
								}
							}
							else if(Conditions[c].GetType() == CAsset_TilingMaterial::CONDITIONTYPE_NOBORDER)
							{
								if(
									i + Conditions[c].GetRelPosX() <= 0 ||
									i + Conditions[c].GetRelPosX() >= Tiles.get_width()-1 ||
									j + Conditions[c].GetRelPosY() <= 0 ||
									j + Conditions[c].GetRelPosY() >= Tiles.get_height()-1
								)
								{
									IsValidRule = false;
									break;
								}
							}
						}
						
						float Probability = pMaterial->GetRuleProbability(*RuleIter);
						if(IsValidRule && (Probability >= 1.0f || Distribution(RandomEngine) < Probability))
						{
							Index = pMaterial->GetRuleTileIndex(*RuleIter);
							Flags = pMaterial->GetRuleTileFlags(*RuleIter);
							break;
						}
					}
					
					Tiles.get_clamp(i, j).SetIndex(Index);
					Tiles.get_clamp(i, j).SetFlags(Flags);
				}
			}
		}
	}
	else
	{
		for(int j=0; j<Tiles.get_height(); j++)
		{
			for(int i=0; i<Tiles.get_width(); i++)
			{
				if(Tiles.get_clamp(i, j).GetBrush() > 0)
					Tiles.get_clamp(i, j).SetIndex(1);
				else
					Tiles.get_clamp(i, j).SetIndex(0);
				Tiles.get_clamp(i, j).SetFlags(0x0);
			}
		}
	}
}