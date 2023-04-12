import { useEffect, useState, useRef } from 'react'
import { useParams } from 'react-router-dom';
import useAxiosPrivate from './hooks/useAxiosPrivate';
import Grid from '@mui/material/Grid';
import ChapterCard from './Components/ChapterCard';


export interface Chapter {
    id: number;
    seriesId?: number;
    name: string;
    pages: number;
}

export default function ChaptersPage() {
    let { id } = useParams();

    const axiosPrivate = useAxiosPrivate();

    const [chapters, setChapters] = useState<Chapter[]>([])

    useEffect(() => {
        let isMounted = true;
        const controller = new AbortController();

        const getChapters = async () => {
            try {
                const response = await axiosPrivate.get(`/api/series/info/${id}`, {
                    signal: controller.signal
                });
                console.log(response.data);
                if (isMounted) {
                    setChapters(response.data as Chapter[]);
                    setChapters(x => x.map(y => Object.assign(y, { seriesId: parseInt(id!!) })));
                    console.log(chapters);
                }
            } catch (err) {
                console.error(err);
                // navigate('/login', { state: { from: location }, replace: true });
            }
        }

        getChapters();

        return () => {
            isMounted = false;
            controller.abort();
        }
    }, [])

    return (<div>
        <Grid container spacing={2} direction="row">
            {
                chapters.map((item, i) => (
                    <Grid item key={i}>
                        <ChapterCard chapter={item} />
                    </Grid>))
            }
        </Grid>
    </div>)
}
