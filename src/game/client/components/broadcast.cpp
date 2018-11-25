/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include <engine/graphics.h>
#include <engine/textrender.h>
#include <generated/protocol.h>
#include <generated/client_data.h>

#include <game/client/gameclient.h>

#include "broadcast.h"
#include "chat.h"
#include "scoreboard.h"
#include "motd.h"

inline bool IsCharANum(char c)
{
	return c >= '0' && c <= '9';
}

inline int WordLengthBack(const char *pText, int MaxChars)
{
	int s = 0;
	while(MaxChars--)
	{
		if((*pText == '\n' || *pText == '\t' || *pText == ' '))
			return s;
		pText--;
		s++;
	}
	return 0;
}

inline bool IsCharWhitespace(char c)
{
	return c == '\n' || c == '\t' || c == ' ';
}

void CBroadcast::RenderServerBroadcast()
{
	if(!g_Config.m_ClShowServerBroadcast || m_pClient->m_MuteServerBroadcast)
		return;

	const bool ColoredBroadcastEnabled = g_Config.m_ClEnableColoredBroadcast;
	const float Height = 300;
	const float Width = Height*Graphics()->ScreenAspect();

	const float DisplayDuration = 10.0f;
	const float DisplayStartFade = 9.0f;
	const float DeltaTime = Client()->LocalTime() - m_SrvBroadcastReceivedTime;

	if(m_aSrvBroadcastMsg[0] == 0 || DeltaTime > DisplayDuration)
		return;

	if(m_pClient->m_pChat->IsActive())
			return;

	const float Fade = 1.0f - max(0.0f, (DeltaTime - DisplayStartFade) / (DisplayDuration - DisplayStartFade));

	CUIRect ScreenRect = {0, 0, Width, Height};

	CUIRect BcView = ScreenRect;
	BcView.x += Width * 0.25f;
	BcView.y += Height * 0.8f;
	BcView.w *= 0.5f;
	BcView.h *= 0.2f;

	float FontSize = 11.0f;

	vec4 ColorTop(0, 0, 0, 0);
	vec4 ColorBot(0, 0, 0, 0.4f * Fade);
	CUIRect BgRect;
	BcView.HSplitBottom(10.0f, 0, &BgRect);

	RenderTools()->DrawUIRect4(&BgRect, ColorTop, ColorTop,
							   ColorBot, ColorBot, 0, 0);

	// server broadcast line
	CUIRect TitleRect;
	BcView.HSplitBottom(10.0f, &BcView, &TitleRect);
	TitleRect.y += 1.5f;
	TextRender()->TextColor(1, 1, 1, 0.6f * Fade);
	UI()->DoLabel(&TitleRect, Localize("Server broadcast"), 5.5f, CUI::ALIGN_CENTER);

	BcView.VMargin(5.0f, &BcView);
	BcView.HSplitBottom(2.0f, &BcView, 0);

	const char* pBroadcastMsg = m_aSrvBroadcastMsg;
	const int MsgLen = m_aSrvBroadcastMsgLen;

	// broadcast message
	// one line == big font
	// > one line == small font

	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, BcView.x, BcView.y, FontSize, 0);
	Cursor.m_LineWidth = BcView.w;
	TextRender()->TextEx(&Cursor, pBroadcastMsg, -1);

	// can't fit on one line, reduce size
	if(Cursor.m_LineCount > 1)
	{
		FontSize = 6.5f; // smaller font
		TextRender()->SetCursor(&Cursor, BcView.x, BcView.y, FontSize, 0);
		Cursor.m_LineWidth = BcView.w;
		TextRender()->TextEx(&Cursor, pBroadcastMsg, -1);
	}

	// make lines
	struct CLineInfo
	{
		const char* m_pStrStart;
		int m_StrLen;
		float m_Width;
	};

	CLineInfo aLines[10];
	int CurCharCount = 0;
	int LineCount = 0;

	while(CurCharCount < MsgLen)
	{
		const char* RemainingMsg = pBroadcastMsg + CurCharCount;

		TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = BcView.w;

		TextRender()->TextEx(&Cursor, RemainingMsg, -1);
		int StrLen = Cursor.m_CharCount;

		// don't cut words
		if(CurCharCount + StrLen < MsgLen)
		{
			const int WorldLen = WordLengthBack(RemainingMsg + StrLen, StrLen);
			if(WorldLen > 0 && WorldLen < StrLen)
			{
				StrLen -= WorldLen;
				TextRender()->SetCursor(&Cursor, 0, 0, FontSize, TEXTFLAG_STOP_AT_END);
				Cursor.m_LineWidth = BcView.w;
				TextRender()->TextEx(&Cursor, RemainingMsg, StrLen);
			}
		}

		const float TextWidth = Cursor.m_X-Cursor.m_StartX;

		CLineInfo Line = { RemainingMsg, StrLen, TextWidth };
		aLines[LineCount++] = Line;
		CurCharCount += StrLen;
	}

	// draw lines
	TextRender()->TextColor(1, 1, 1, 1);
	TextRender()->TextOutlineColor(0, 0, 0, 0.3f);
	float y = BcView.y + BcView.h - LineCount * FontSize;

	for(int l = 0; l < LineCount; l++)
	{
		const CLineInfo& Line = aLines[l];
		TextRender()->SetCursor(&Cursor, BcView.x + (BcView.w - Line.m_Width) * 0.5f, y,
								FontSize, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = BcView.w;

		// draw colored text
		if(ColoredBroadcastEnabled)
		{
			int DrawnStrLen = 0;
			int ThisCharPos = Line.m_pStrStart - pBroadcastMsg;
			while(DrawnStrLen < Line.m_StrLen)
			{
				int StartColorID = -1;
				int ColorStrLen = -1;

				for(int j = 0; j < m_SrvBroadcastColorCount; j++)
				{
					if((ThisCharPos+DrawnStrLen) >= m_aSrvBroadcastColorList[j].m_CharPos)
					{
						StartColorID = j;
					}
					else if(StartColorID >= 0)
					{
						ColorStrLen = m_aSrvBroadcastColorList[j].m_CharPos - m_aSrvBroadcastColorList[StartColorID].m_CharPos;
						break;
					}
				}

				dbg_assert(StartColorID >= 0, "This should not be -1, color not found");

				if(ColorStrLen == -1)
					ColorStrLen = Line.m_StrLen-DrawnStrLen;

				const CBcColor& TextColor = m_aSrvBroadcastColorList[StartColorID];
				float r = TextColor.m_R/255.f;
				float g = TextColor.m_G/255.f;
				float b = TextColor.m_B/255.f;
				float AvgLum = 0.2126*r + 0.7152*g + 0.0722*b;

				if(AvgLum < 0.25f)
					TextRender()->TextOutlineColor(1, 1, 1, 0.6f);
				else
					TextRender()->TextOutlineColor(0, 0, 0, 0.3f);

				TextRender()->TextColor(r, g, b, Fade);

				TextRender()->TextEx(&Cursor, Line.m_pStrStart+DrawnStrLen, ColorStrLen);
				DrawnStrLen += ColorStrLen;
			}
		}
		else
		{
			TextRender()->TextEx(&Cursor, Line.m_pStrStart, Line.m_StrLen);
		}

		y += FontSize;
	}

	TextRender()->TextColor(1, 1, 1, 1);
	TextRender()->TextOutlineColor(0, 0, 0, 0.3f);
}

CBroadcast::CBroadcast()
{
	OnReset();
}

void CBroadcast::DoBroadcast(const char *pText)
{
	str_copy(m_aBroadcastText, pText, sizeof(m_aBroadcastText));
	CTextCursor Cursor;
	TextRender()->SetCursor(&Cursor, 0, 0, 12.0f, TEXTFLAG_STOP_AT_END);
	Cursor.m_LineWidth = 300*Graphics()->ScreenAspect();
	TextRender()->TextEx(&Cursor, m_aBroadcastText, -1);
	m_BroadcastRenderOffset = 150*Graphics()->ScreenAspect()-Cursor.m_X/2;
	m_BroadcastTime = Client()->LocalTime() + 10.0f;
}

void CBroadcast::OnReset()
{
	m_BroadcastTime = 0;
}

void CBroadcast::OnMessage(int MsgType, void* pRawMsg)
{
	// process server broadcast message
	if(MsgType == NETMSGTYPE_SV_BROADCAST && g_Config.m_ClShowServerBroadcast &&
	   !m_pClient->m_MuteServerBroadcast)
	{
		CNetMsg_Sv_Broadcast *pMsg = (CNetMsg_Sv_Broadcast *)pRawMsg;

		// new broadcast message
		int MsgLen = str_length(pMsg->m_pMessage);
		mem_zero(m_aSrvBroadcastMsg, sizeof(m_aSrvBroadcastMsg));
		m_aSrvBroadcastMsgLen = 0;
		m_SrvBroadcastReceivedTime = Client()->LocalTime();

		const CBcColor White = { 255, 255, 255, 0 };
		m_aSrvBroadcastColorList[0] = White;
		m_SrvBroadcastColorCount = 1;

		// parse colors
		for(int i = 0; i < MsgLen; i++)
		{
			const char* c = pMsg->m_pMessage + i;
			const char* pTmp = c;
			int CharUtf8 = str_utf8_decode(&pTmp);
			const int Utf8Len = pTmp-c;

			if(*c == CharUtf8 && *c == '^')
			{
				if(i+3 < MsgLen && IsCharANum(c[1]) && IsCharANum(c[2])  && IsCharANum(c[3]))
				{
					u8 r = (c[1] - '0') * 24 + 39;
					u8 g = (c[2] - '0') * 24 + 39;
					u8 b = (c[3] - '0') * 24 + 39;
					CBcColor Color = { r, g, b, m_aSrvBroadcastMsgLen };
					if(m_SrvBroadcastColorCount < MAX_BROADCAST_COLORS)
						m_aSrvBroadcastColorList[m_SrvBroadcastColorCount++] = Color;
					i += 3;
					continue;
				}
			}

			if(m_aSrvBroadcastMsgLen+Utf8Len < MAX_BROADCAST_MSG_LENGTH)
				m_aSrvBroadcastMsg[m_aSrvBroadcastMsgLen++] = *c;
		}
	}
}

void CBroadcast::OnRender()
{
	if(m_pClient->m_pScoreboard->Active() || m_pClient->m_pMotd->IsActive())
		return;

	Graphics()->MapScreen(0, 0, 300*Graphics()->ScreenAspect(), 300);

	// client broadcast
	if(Client()->LocalTime() < m_BroadcastTime)
	{
		CTextCursor Cursor;
		TextRender()->SetCursor(&Cursor, m_BroadcastRenderOffset, 40.0f, 12.0f, TEXTFLAG_RENDER|TEXTFLAG_STOP_AT_END);
		Cursor.m_LineWidth = 300*Graphics()->ScreenAspect()-m_BroadcastRenderOffset;
		TextRender()->TextEx(&Cursor, m_aBroadcastText, -1);
	}

	// server broadcast
	RenderServerBroadcast();
}

