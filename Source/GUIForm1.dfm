object GUIForm1: TGUIForm1
  Left = 227
  Top = 123
  Width = 902
  Height = 639
  Caption = 'ST RecoverPlus'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  PixelsPerInch = 96
  TextHeight = 13
  object Label1: TLabel
    Left = 8
    Top = 475
    Width = 55
    Height = 13
    Caption = 'Whole disk:'
  end
  object LabelInformation: TLabel
    Left = 278
    Top = 475
    Width = 56
    Height = 13
    Caption = 'Information'
  end
  object LabelElapsedTime: TLabel
    Left = 812
    Top = 475
    Width = 62
    Height = 13
    Caption = 'Elapsed time'
  end
  object ComboBoxDisk: TComboBox
    Left = 12
    Top = 7
    Width = 32
    Height = 21
    Style = csDropDownList
    ItemHeight = 13
    ItemIndex = 0
    TabOrder = 5
    Text = 'A:'
    Items.Strings = (
      'A:'
      'B:')
  end
  object ButtonReadDisk: TButton
    Left = 48
    Top = 7
    Width = 169
    Height = 25
    Caption = 'Read disk and save image file'
    TabOrder = 0
    OnClick = ButtonReadDiskClick
  end
  object ButtonAnalyse: TButton
    Left = 222
    Top = 7
    Width = 122
    Height = 25
    Caption = 'Analyse surface'
    TabOrder = 1
    OnClick = ButtonAnalyseClick
  end
  object CheckBoxSaveRawTrackInfos: TCheckBox
    Left = 355
    Top = 19
    Width = 185
    Height = 13
    Caption = 'Save track information (debug)'
    TabOrder = 7
  end
  object BitBtnCancel: TBitBtn
    Left = 744
    Top = 7
    Width = 136
    Height = 25
    Enabled = False
    TabOrder = 2
    OnClick = BitBtnCancelClick
    Kind = bkAbort
  end
  object PageControlResults: TPageControl
    Left = 0
    Top = 37
    Width = 886
    Height = 431
    ActivePage = TabSheetSides
    TabIndex = 1
    TabOrder = 3
    object TabSheetSidesGrids: TTabSheet
      Caption = 'Tables'
      object DrawGridSideASectors: TDrawGrid
        Left = 2
        Top = 2
        Width = 876
        Height = 200
        ColCount = 85
        DefaultColWidth = 9
        DefaultRowHeight = 14
        DefaultDrawing = False
        FixedCols = 0
        RowCount = 12
        FixedRows = 0
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnDrawCell = DrawGridSideASectorsDrawCell
        OnMouseMove = DrawGridSideASectorsMouseMove
      end
      object DrawGridSideBSectors: TDrawGrid
        Left = 2
        Top = 204
        Width = 876
        Height = 200
        ColCount = 85
        DefaultColWidth = 9
        DefaultRowHeight = 14
        DefaultDrawing = False
        FixedCols = 0
        RowCount = 12
        FixedRows = 0
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
        OnDrawCell = DrawGridSideASectorsDrawCell
        OnMouseMove = DrawGridSideASectorsMouseMove
      end
    end
    object TabSheetSides: TTabSheet
      Caption = 'Surface'
      ImageIndex = 1
      object ImageSide0: TImage
        Left = 19
        Top = 0
        Width = 406
        Height = 418
      end
      object ImageSide1: TImage
        Left = 453
        Top = 0
        Width = 406
        Height = 418
      end
    end
  end
  object ComboBoxMaxiTime: TComboBox
    Left = 67
    Top = 472
    Width = 191
    Height = 21
    Style = csDropDownList
    ItemHeight = 13
    ItemIndex = 2
    TabOrder = 4
    Text = 'Giveup after 30 mn (Recommended)'
    Items.Strings = (
      'Giveup after 5 mn'
      'Giveup after 15 mn'
      'Giveup after 30 mn (Recommended)'
      'Giveup after 60 mn'
      'Giveup after 2 h'
      'Giveup after 5 h'
      'Never giveup (well, after 20 days)')
  end
  object MemoLOG: TMemo
    Left = 7
    Top = 492
    Width = 873
    Height = 87
    Hint = 'LOG'
    Lines.Strings = (
      'LOG:')
    ScrollBars = ssVertical
    TabOrder = 6
  end
  object SaveDialogDiskImage: TSaveDialog
    DefaultExt = 'ST'
    FileName = 'Disk_.ST'
    Filter = 'ST disk image|*.ST'
    Options = [ofOverwritePrompt, ofHideReadOnly, ofPathMustExist, ofEnableSizing]
    Title = 'Save the disk image .ST'
    Left = 648
    Top = 24
  end
end
