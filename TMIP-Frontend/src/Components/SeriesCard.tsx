import { useState } from 'react'
import { Series } from '../Home'
import { BASE_URL } from '../api/axios';
import Card from '@mui/material/Card';
import CardContent from '@mui/material/CardContent';
import CardMedia from '@mui/material/CardMedia';
import Typography from '@mui/material/Typography';
import CardActionArea from '@mui/material/CardActionArea';
import { Link } from 'react-router-dom';
import Grow from '@mui/material/Grow';

interface Props {
    series: Series;
    onContextMenu: React.MouseEventHandler<HTMLElement>;
}

export default function SeriesCard({ series, onContextMenu }: Props) {
    const [done, setDone] = useState(false);

    return (
        <Grow in={done} onContextMenu={onContextMenu}>
            <Card sx={{ maxWidth: 150 }}>
                <CardActionArea component={Link} to={`/series/${series.id}`}>
                    <CardMedia
                        component="img"
                        alt={series.name}
                        onLoad={() => setDone(true)}
                        onError={() => setDone(true)}
                        src={`${BASE_URL}/api/v2/series/cover/${series.id}`}
                        style={{ height: '210px', width: '150px' }}
                    />
                    <CardContent>
                        <Typography gutterBottom variant="body1" sx={{
                            overflow: "hidden",
                            textOverflow: "ellipsis",
                            display: "-webkit-box",
                            WebkitLineClamp: "2",
                            WebkitBoxOrient: "vertical",
                            minHeight: '4.5rem',
                            marginBottom: '0.5rem',
                        }}>
                            {series.name}
                        </Typography>
                        <Typography gutterBottom variant="body2">
                            {series.chapters_count} Books
                        </Typography>
                    </CardContent>
                </CardActionArea>
            </Card>
        </Grow>
    )
}
