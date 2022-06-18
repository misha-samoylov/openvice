float4 PS( float4 Pos : SV_POSITION ) : SV_TARGET
{
    // Возвращаем желтый цвет, непрозрачный (альфа == 1, альфа-канал не включен).
	return float4(1.0f, 1.0f, 0.0f, 1.0f); 
}