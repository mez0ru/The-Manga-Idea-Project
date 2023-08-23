import { create } from 'zustand'
import { Chapter } from '../ChaptersPage'

export type State = {
    chapters: Chapter[],
    selectedChapter: number,
    seriesName: string,
    scrollPosition: number,
}

export type Actions = {
    setChapters: (chapters: Chapter[]) => void
    setSelectedChapter: (chapterIndex: number) => void
    setName: (seriesName: string) => void
    getNextChapter: (chapterId: number) => Chapter | undefined
    getPrevChapter: (chapterId: number) => Chapter | undefined
    reset: () => void
}

const initialState: State = {
    chapters: [],
    selectedChapter: -1,
    seriesName: '',
    scrollPosition: 0,
}

export const useChaptersStore = create<State & Actions>()((set, get) => ({
    ...initialState,

    setChapters: (chapters: Chapter[]) => {
        set({ chapters })
    },

    setSelectedChapter: (chapter: number) => {
        set({ selectedChapter: chapter })
    },

    setName: (name: string) => {
        set({ seriesName: name })
    },

    getNextChapter: (chapterId: number) => {
        let current = get().chapters.findIndex((x) => x.id === chapterId)
        if (current !== -1 && ++current < get().chapters.length)
            return get().chapters[current];
        else
            return undefined;
    },

    getPrevChapter: (chapterId: number) => {
        let current = get().chapters.findIndex((x) => x.id === chapterId)
        if (current !== -1 && --current > -1)
            return get().chapters[current];
        else
            return undefined;
    },

    reset: () => {
        set(initialState)
    },
}))
